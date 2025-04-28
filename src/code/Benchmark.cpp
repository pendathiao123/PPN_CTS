#include "Benchmark.h"
#include "Client.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    int nb_transactions = 10;
    Client client;
    client.StartClient("127.0.0.1", 4433, "1206", "4aeeb5ffc3be24aae06916942890aad97b6bb81572cdc05ee1a3462186675057");
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < nb_transactions; ++i) {
        std::string currency = "SRD-BTC";
        double amount = 1;

        if (i % 2 == 0) {
            std::cout << "Transaction " << i + 1 << ": Achat de " << currency << std::endl;
            client.buy(currency, amount);
        } else {
            std::cout << "Transaction " << i + 1 << ": Vente de " << currency << std::endl;
            client.sell(currency, amount);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Calcul de la durée
    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
    return 0;
}