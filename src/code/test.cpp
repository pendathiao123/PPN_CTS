#include <iostream>
#include <memory>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../headers/ServerConnection.h"
#include "../headers/ClientInitiator.h"

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 4433;

// Callback pour déboguer OpenSSL
void openssl_debug_callback(const SSL* ssl, int where, int ret) {
    const char* state = SSL_state_string_long(ssl);
    const char* desc = SSL_alert_desc_string_long(ret);
    const char* whereStr = (where & SSL_CB_HANDSHAKE_START) ? "HANDSHAKE START" :
                           (where & SSL_CB_HANDSHAKE_DONE) ? "HANDSHAKE DONE" :
                           (where & SSL_CB_ALERT) ? "ALERT" : "OTHER";
    std::cerr << "[OpenSSL DEBUG] State: " << state
              << ", Desc: " << desc
              << ", Where: " << whereStr
              << ", Ret: " << ret << std::endl;
}

SSL_CTX* initialize_ssl_context(const char* ca_cert_path) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        std::cerr << "[ERROR] Failed to initialize SSL context" << std::endl;
        ERR_print_errors_fp(stderr);
        return nullptr;
    }

    // Configuration de la vérification des certificats
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

    // Charger les certificats CA
    if (SSL_CTX_load_verify_locations(ctx, ca_cert_path, nullptr) != 1) {
        std::cerr << "[ERROR] Failed to load CA certificates from " << ca_cert_path << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return nullptr;
    }

    // Ajouter le callback de débogage
    SSL_CTX_set_info_callback(ctx, openssl_debug_callback);

    return ctx;
}

int main() {
    // Initialisation de la bibliothèque SSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const char* ca_cert_path = "../../rootCA.crt";
    SSL_CTX* ctx = initialize_ssl_context(ca_cert_path);
    if (!ctx) {
        return 1; // Échec de l'initialisation du contexte SSL
    }

    try {
        ClientInitiator clientInitiator;
        auto connection = clientInitiator.ConnectToServer(SERVER_IP, SERVER_PORT, ctx);

        if (!connection || !connection->isConnected()) {
            throw std::runtime_error("Failed to connect to server");
        }

        // Authentification
        std::string id = "6703";
        std::string token = "f1b86ffe5e9a4620b93f8345cf823dcd8fb43589586674e2e2592c9a12ab139c";
        std::string authMessage = "ID:" + id + ",TOKEN:" + token + "\n";
        std::cout << "[DEBUG] Sending authentication message: " << authMessage << std::endl;

        connection->send(authMessage);

        std::string authResponse = connection->receiveLine();
        std::cout << "[DEBUG] Authentication Response: " << authResponse << std::endl;

        if (authResponse.find("AUTH SUCCESS") == std::string::npos) {
            throw std::runtime_error("Authentication failed: " + authResponse);
        }

        // Envoi d'une transaction
        std::string transaction = "BUY SRD-BTC 5\n";
        std::cout << "[DEBUG] Sending transaction: " << transaction << std::endl;

        connection->send(transaction);

        std::string transactionResponse = connection->receiveLine();
        std::cout << "[DEBUG] Transaction Response: " << transactionResponse << std::endl;

        // Fermeture de la connexion
        connection->closeConnection();
        std::cout << "[DEBUG] Connection closed successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception caught: " << e.what() << std::endl;
        ERR_print_errors_fp(stderr);
    }

    // Libération du contexte SSL
    SSL_CTX_free(ctx);
    return 0;
}
