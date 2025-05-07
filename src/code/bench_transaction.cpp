#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../headers/ServerConnection.h"
#include "../headers/ClientInitiator.h"

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 4433;
constexpr int NUM_CLIENTS = 3;
constexpr int TRANSACTIONS_PER_CLIENT = 100;

std::atomic<int> successful_transactions(0);
std::atomic<int> failed_transactions(0);
std::mutex cout_mutex;
#include <fstream>

void save_results_to_csv(const std::string& filename, int num_transactions, double tps, double duration, int success, int fail) {
    std::ofstream file;
    file.open(filename, std::ios::app); // Ouvre le fichier en mode ajout
    if (!file.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << "\n";
        return;
    }

    // Écrit les résultats dans le fichier
    file << num_transactions << "," << tps << "," << duration << "," << success << "," << fail << "\n";
    file.close();
}


void send_transactions(int client_id, SSL_CTX* ctx) {
    ClientInitiator clientInitiator;
    std::shared_ptr<ServerConnection> connection;

    // Liste des IDs et tokens valides
    const std::vector<std::pair<std::string, std::string>> credentials = {
        {"6703", "f1b86ffe5e9a4620b93f8345cf823dcd8fb43589586674e2e2592c9a12ab139c"},
        {"client2", "token2"}, // Ajoutez d'autres IDs et tokens si nécessaire
        {"client3", "token3"}
    };

    try {
        connection = clientInitiator.ConnectToServer(SERVER_IP, SERVER_PORT, ctx);
        if (!connection || !connection->isConnected()) {
            throw std::runtime_error("Failed to connect to server");
        }

        // Authentification avec l'ID et le token
        const auto& [id, token] = credentials[client_id % credentials.size()];
        std::string authMessage = "ID:" + id + ",TOKEN:" + token + "\n";
        connection->send(authMessage);

        std::string authResponse = connection->receiveLine();
        if (authResponse.find("AUTH SUCCESS") == std::string::npos) {
            throw std::runtime_error("Authentication failed: " + authResponse);
        }

        // Envoi des transactions après authentification réussie
        for (int i = 0; i < TRANSACTIONS_PER_CLIENT; ++i) {
            std::string transaction = (i % 2 == 0) ? "BUY SRD-BTC 5\n" : "SELL SRD-BTC 5\n";
            try {
                connection->send(transaction);
                std::string response = connection->receiveLine();
                if (response.find("TRANSACTION_RESULT") != std::string::npos) {
                    successful_transactions++;
                } else {
                    failed_transactions++;
                }
            } catch (...) {
                failed_transactions++;
            }
        }

        connection->closeConnection();
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Client #" << client_id << " failed: " << e.what() << std::endl;
        failed_transactions++;
    }
}

int main() {
    SSL_library_init();
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        std::cerr << "Failed to initialize SSL context" << std::endl;
        return 1;
    }

    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_CLIENTS; ++i) {
        threads.emplace_back(send_transactions, i, ctx);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    double tps = successful_transactions / duration.count();

    std::cout << "=== Benchmark Results ===" << std::endl;
    std::cout << "Total Transactions: " << NUM_CLIENTS * TRANSACTIONS_PER_CLIENT << std::endl;
    std::cout << "Successful Transactions: " << successful_transactions.load() << std::endl;
    std::cout << "Failed Transactions: " << failed_transactions.load() << std::endl;
    std::cout << "Duration: " << duration.count() << " seconds" << std::endl;
    std::cout << "Transactions Per Second (TPS): " << tps << std::endl;

    SSL_CTX_free(ctx);
    return 0;
}