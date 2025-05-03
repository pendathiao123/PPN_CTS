#include "../headers/Server.h"
#include "../headers/TransactionQueue.h" 
#include "../headers/Logger.h" 

#include <iostream> 
#include <string>   
#include <vector>   
#include <stdexcept> 
#include <memory>   

// --- Initialisation globale OpenSSL ---
void initialize_openssl() {
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    SSL_load_error_strings();
}

void cleanup_openssl() {
    EVP_cleanup();
    ERR_free_strings(); // Libère la table des chaînes d'erreur
}

// --- Définition de l'instance globale de la TransactionQueue ---
TransactionQueue txQueue; // Unique instance de la file de transactions globale.


// --- Fonction main : Point d'entrée du programme serveur ---
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    LOG("Main_Serv INFO : Démarrage du programme serveur (config hardcodée).", "INFO");

    // --- Configuration Hardcodée ---
    int port = 4433; // Port d'écoute hardcodé
    std::string certFile = "../server.crt"; // Chemin du certificat hardcodé
    std::string keyFile = "../server.key";   // Chemin de la clé privée hardcodé
    std::string usersFile = "users.txt"; // Chemin du fichier utilisateurs hardcodé
    std::string transactionCounterFile = "counter.txt"; // Chemin du compteur hardcodé
    std::string transactionHistoryFile = "../src/data/global_transactions.csv"; // Chemin historique hardcodé
    std::string walletsDir = "../src/data/wallets/"; // Répertoire portefeuilles hardcodé

    LOG("Main_Serv INFO : Configuration chargée (hardcodée). Port: " + std::to_string(port) + ", Cert: " + certFile + ", Key: " + keyFile + ", Users: " + usersFile + ", Counter: " + transactionCounterFile + ", History: " + transactionHistoryFile + ", Wallets Dir: " + walletsDir, "INFO");


    // --- Initialisation OpenSSL ---
    initialize_openssl();


    // --- Création et Démarrage du Serveur ---
    std::shared_ptr<Server> server = nullptr;
    try {
        // Utilise les valeurs hardcodées pour créer l'objet Server.
        server = std::make_shared<Server>(port, certFile, keyFile, usersFile,
                                          transactionCounterFile, transactionHistoryFile, walletsDir);

        LOG("Main_Serv INFO : Objet Server créé. Démarrage...", "INFO");
        server->StartServer(); // Cette méthode bloquera jusqu'à l'arrêt.

    } catch (const std::exception& e) {
        LOG("Main_Serv CRITICAL : Exception non gérée lors de la création ou du démarrage du serveur. Exception: " + std::string(e.what()), "CRITICAL");
        if (server) {
             server->StopServer(); // Tente un arrêt propre.
        }
        return 1; // Code d'erreur.

    } catch (...) {
         LOG("Main_Serv CRITICAL : Exception inconnue non gérée lors de la création ou du démarrage du serveur.", "CRITICAL");
         if (server) {
              server->StopServer();
         }
         return 1; // Code d'erreur.
    }

    // StartServer() bloque normalement. Ce code n'est atteint que si StartServer() retourne.
    LOG("Main_Serv INFO : Programme serveur terminé normalement.", "INFO");

    // --- Nettoyage OpenSSL ---
    cleanup_openssl();

    return 0; // Code de succès.
}