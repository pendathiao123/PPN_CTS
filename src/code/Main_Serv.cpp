// src/code/Main_Serv.cpp - Point d'entrée de l'exécutable serveur

#include "../headers/Server.h" // Inclut la classe Server
#include "../headers/TransactionQueue.h" // Déclare extern la file de transactions globale
#include "../headers/Logger.h" // Pour la macro LOG

#include <iostream> // std::cout, std::cerr
#include <string>   // std::string
#include <vector>   // std::vector
#include <stdexcept> // std::runtime_error
#include <memory>   // std::shared_ptr, std::make_shared

// --- Initialisation globale OpenSSL ---
void initialize_openssl() {
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    SSL_load_error_strings();
    // RAND_poll();
}

void cleanup_openssl() {
    EVP_cleanup();
    // SSL_COMP_free_compression_methods();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();
}

// --- Définition de l'instance globale de la TransactionQueue ---
TransactionQueue txQueue; // Unique instance de la file de transactions globale.


// --- Fonction main : Point d'entrée du programme serveur ---
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    LOG("Main_Serv INFO : Démarrage du programme serveur (config hardcodée).", "INFO");

    // --- Configuration Hardcodée ---
    // REMPLACEZ CES VALEURS PAR VOS CHEMINS ET PORT RÉELS POUR LE TEST
    int port = 4433; // Port d'écoute hardcodé
    std::string certFile = "../server.crt"; // Chemin du certificat hardcodé
    std::string keyFile = "../server.key";   // Chemin de la clé privée hardcodé
    std::string usersFile = "users.txt"; // Chemin du fichier utilisateurs hardcodé
    std::string transactionCounterFile = "counter.txt"; // Chemin du compteur hardcodé
    std::string transactionHistoryFile = "../src/data/global_transactions.csv"; // Chemin historique hardcodé
    std::string walletsDir = "../src/data/wallets/"; // Répertoire portefeuilles hardcodé
    // ASSUREZ-VOUS QUE CES FICHIERS ET RÉPERTOIRES EXISTENT OU SERONT CRÉÉS COMME PRÉVU.

    // Le parsing des arguments de ligne de commande est ignoré/retiré ici.
    LOG("Main_Serv INFO : Configuration chargée (hardcodée). Port: " + std::to_string(port) + ", Cert: " + certFile + ", Key: " + keyFile + ", Users: " + usersFile + ", Counter: " + transactionCounterFile + ", History: " + transactionHistoryFile + ", Wallets Dir: " + walletsDir, "INFO");


    // --- Initialisation OpenSSL ---
    LOG("Main_Serv DEBUG : Initialisation globale OpenSSL...", "DEBUG");
    initialize_openssl();
    LOG("Main_Serv DEBUG : Initialisation globale OpenSSL terminée.", "DEBUG");


    // --- Création et Démarrage du Serveur ---
    std::shared_ptr<Server> server = nullptr;
    try {
        // Utilise les valeurs hardcodées pour créer l'objet Server.
        server = std::make_shared<Server>(port, certFile, keyFile, usersFile,
                                          transactionCounterFile, transactionHistoryFile, walletsDir);

        LOG("Main_Serv INFO : Objet Server créé. Démarrage...", "INFO");
        server->StartServer(); // Cette méthode bloquera jusqu'à l'arrêt (via signal non géré ou implémenté).

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

    // StartServer() bloque normalement. Ce code n'est atteint que si StartServer() retourne,
    // ce qui n'arrive pas avec la boucle acceptThread.join() sauf en cas d'arrêt brutal ou de signal.
    LOG("Main_Serv INFO : Programme serveur terminé normalement.", "INFO");

    // --- Nettoyage OpenSSL ---
    LOG("Main_Serv DEBUG : Nettoyage global OpenSSL...", "DEBUG");
    cleanup_openssl();
    LOG("Main_Serv DEBUG : Nettoyage global OpenSSL terminé.", "DEBUG");

    return 0; // Code de succès.
}

// Note : Pour un arrêt propre par Ctrl+C, une gestion des signaux (SIGINT) est nécessaire.
// Cela complexifierait ce main simple pour l'instant.