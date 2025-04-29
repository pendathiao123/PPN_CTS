#include <iostream>
#include <filesystem> 
#include <stdexcept>  
#include <csignal>   

#include "../headers/Server.h"          
#include "../headers/TransactionQueue.h" 
#include "../headers/Global.h"           
#include "../headers/Logger.h"          
#include "../headers/Transaction.h"  

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <curl/curl.h>


// Déclaration de la file de transactions globale (si elle est gérée comme une variable globale)
// Si tu as fait de txQueue un membre du Server, retire cette ligne.
TransactionQueue txQueue;

// Variable globale pour signaler l'arrêt au serveur principal
// Le gestionnaire de signal modifiera ce flag.
std::atomic<bool> server_running = true;

// Gestionnaire de signal pour arrêter le serveur proprement
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG("Signal d'arrêt reçu (" + std::to_string(signal) + "). Demande d'arrêt du serveur.", "INFO");
        server_running.store(false); // Signale à la boucle principale de s'arrêter
    }
}


int main() {
    // Enregistrer le gestionnaire de signal
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    LOG("Gestionnaires de signaux enregistrés.", "INFO");


    // 1. Initialisation globale des bibliothèques
    // Initialisation OpenSSL (doit être fait une seule fois au démarrage)
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_SSL_strings(); // Charge toutes les chaînes d'erreur OpenSSL (remplace ERR_load_BIO_strings et autres)
    ERR_load_crypto_strings();
    LOG("Bibliothèques OpenSSL initialisées.", "INFO");

    // Initialisation libcurl (doit être fait une seule fois au démarrage si utilisé globalement)
    // C'est le cas car Global utilise libcurl.
    CURLcode res_curl_init = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res_curl_init != CURLE_OK) {
        LOG("Erreur lors de l'initialisation globale de libcurl : " + std::string(curl_easy_strerror(res_curl_init)), "ERROR");
        // Gérer l'erreur fatale si curl est essentiel
        return 1; // Quitter le programme
    }
    LOG("Bibliothèque libcurl initialisée globalement.", "INFO");


    // 2. Vérifie et crée le répertoire des portefeuilles (chemin hardcodé)
    std::string wallet_dir = "../src/data/wallets"; // Chemin hardcodé
    if (!std::filesystem::exists(wallet_dir)) {
        std::error_code ec; // Pour capturer l'erreur au lieu de lancer une exception
        if (std::filesystem::create_directories(wallet_dir, ec)) {
            LOG("Répertoire des portefeuilles créé : " + wallet_dir, "INFO");
        } else {
            LOG("Erreur lors de la création du répertoire des portefeuilles " + wallet_dir + " : " + ec.message(), "ERROR");
            // Continuer, mais logger l'erreur
        }
    } else {
        LOG("Répertoire des portefeuilles trouvé : " + wallet_dir, "INFO");
    }

    // Définir les chemins de fichiers (hardcodés, à rendre configurables)
    std::string cert_file = "../server.crt";
    std::string key_file = "../server.key";
    std::string users_file = "../src/data/users.txt"; // Chemin hardcodé (ton CMake le nommait configFile.csv ?)
    std::string log_file = "../server.log"; // Le Logger gère son propre fichier, ce chemin peut être donné au Logger si besoin
    std::string transaction_counter_file = "../src/data/transaction_counter.txt";
    std::string transaction_history_file = "../src/data/transactions.csv";

    // 3. Charger le compteur de transactions (Transaction::loadCounter attend un chemin)
    Transaction::loadCounter(transaction_counter_file);

    // 4. Création et démarrage du serveur
    // Le constructeur du serveur prend les chemins et le port
    try {
        Server server(4433, // Port hardcodé
                      cert_file,
                      key_file,
                      users_file,
                      log_file, // Ce chemin n'est plus utilisé par Server directement pour le log global, mais peut servir pour configurer le Logger.
                      transaction_counter_file, // Le chemin est passé pour load/saveCounter
                      transaction_history_file); // Le chemin est passé pour logTransactionToCSV

        // server.StartServer() contient la boucle principale accept().
        server.StartServer(); // La boucle interne doit vérifier server_running


    } catch (const std::exception& e) {
        LOG("Exception fatale lors du démarrage ou de l'exécution du serveur : " + std::string(e.what()), "ERROR");
        // Gérer l'arrêt propre ici si une exception se produit avant le signal d'arrêt
        server_running.store(false); // S'assurer que le flag est false
        return 1; // Quitter avec un code d'erreur
    }


    // Le programme atteint ce point quand server.StartServer() retourne,
    // ce qui devrait arriver lorsque le flag server_running devient false.

    LOG("Boucle principale du serveur terminée. Début de l'arrêt propre...", "INFO");

    // Nettoyage global des bibliothèques
    // Nettoyage libcurl
    curl_global_cleanup();
    LOG("Bibliothèque libcurl nettoyée globalement.", "INFO");

    // Nettoyage OpenSSL
    // Ces appels doivent correspondre aux initialisations
    ERR_free_strings();
    EVP_cleanup();
    LOG("Bibliothèques OpenSSL nettoyées globalement.", "INFO");


    LOG("Programme serveur terminé.", "INFO");
    return 0; // Quitter avec succès
}