// src/code/Main_Cli.cpp - Point d'entrée de l'exécutable client

// Includes standards et système nécessaires
#include <iostream>      // std::cout, std::cerr, std::cin, std::getline
#include <string>        // std::string
#include <vector>        // std::vector
#include <stdexcept>     // std::runtime_error
#include <memory>        // std::shared_ptr, std::make_shared, std::unique_ptr
#include <limits>        // std::numeric_limits
#include <cstdlib>       // EXIT_FAILURE, EXIT_SUCCESS
#include <cstring>       // strerror
#include <algorithm>     // std::transform, std::min
#include <cerrno>        // errno

// Includes OpenSSL spécifiques
#include <openssl/ssl.h>   // SSL
#include <openssl/err.h>   // Erreurs OpenSSL
#include <openssl/rand.h>  // RAND_poll (si besoin)

// Includes spécifiques au projet (assurez-vous que les chemins sont corrects)
#include "../headers/ClientInitiator.h" // Logique d'initiation connexion/contexte SSL client
#include "../headers/ServerConnection.h" // Encapsule la connexion Socket+SSL (DOIT AVOIR receiveLine)
#include "../headers/Logger.h"           // Système de log
#include "../headers/Utils.h"            // Fonctions utilitaires (si utilisées)
#include "../headers/Transaction.h"      // Structures de transaction (si utilisées par le client)
#include "../headers/OpenSSLDeleters.h" // Gestion RAII OpenSSL (pour UniqueSSLCTX)


// Constantes et macros
#define RECEIVE_BUFFER_SIZE 4096 // Taille du buffer pour receiveLine (peut être interne à ServerConnection)

// --- Initialisation globale OpenSSL (côté client) ---
void initialize_openssl_client() {
    LOG("Main_Cli DEBUG : Initialisation globale OpenSSL client...", "DEBUG");
    // Load encryption & hashing algorithms
    OpenSSL_add_all_algorithms();
    // Load SSL/TLS algorithms
    SSL_library_init(); // Obsolète dans les versions très récentes, mais compatible
    // Load error strings
    SSL_load_error_strings();
    // Seed the PRNG if needed (modern OpenSSL versions often seed automatically)
    // RAND_poll();
    LOG("Main_Cli DEBUG : Initialisation globale OpenSSL client terminée.", "DEBUG");
}

// --- Nettoyage global OpenSSL (côté client) ---
void cleanup_openssl_client() {
    LOG("Main_Cli INFO : Nettoyage global OpenSSL client...", "INFO");
    EVP_cleanup();
    // SSL_COMP_free_compression_methods(); // Si pas de compression utilisée
    CRYPTO_cleanup_all_ex_data(); // Libère les données d'index allouées par CRYPTO_get_ex_new_index
    ERR_free_strings(); // Libère la table des chaînes d'erreur
    LOG("Main_Cli INFO : Nettoyage global OpenSSL client terminé.", "INFO");
}

// --- Boucle de Commandes Interactive ---
// Gère la lecture des commandes utilisateur, l'envoi au serveur et l'affichage des réponses.
// Ne retourne que lorsque l'utilisateur tape QUIT ou qu'une erreur réseau survient.
void start_command_loop(std::shared_ptr<ServerConnection> connection) {
    std::cout << "Connecté. Entrez vos commandes (tapez 'QUIT' pour déconnecter) :\n";
    std::string command;

    // Boucle de lecture des commandes sur stdin et envoi au serveur
    while (connection && connection->isConnected()) { // Continuer tant que la connexion est active
        std::cout << "> "; // Invite de commande
        // Lit une ligne entière depuis l'entrée standard (stdin).
        // std::getline échoue en cas de fin de fichier (Ctrl+D) ou erreur de lecture.
        if (!std::getline(std::cin, command)) {
            LOG("Main_Cli INFO : Échec de lecture de la commande (stdin) ou fin de fichier. Déconnexion.", "INFO");
            break; // Sort de la boucle pour gérer la déconnexion
        }

        // Enlever les espaces blancs et les retours chariot/sauts de ligne en début et fin.
        command.erase(0, command.find_first_not_of(" \t\n\r\f\v"));
        command.erase(command.find_last_not_of(" \t\n\r\f\v") + 1);

        if (command.empty()) {
            continue; // Ignore les commandes vides
        }

        // La commande QUIT est gérée côté client pour quitter la boucle interactive.
        // Elle est aussi envoyée au serveur pour lui signaler la déconnexion propre.
        if (command == "QUIT") {
            LOG("Main_Cli INFO : Commande QUIT reçue. Déconnexion demandée par l'utilisateur.", "INFO");
            // Ne pas break immédiatement, envoyer QUIT au serveur d'abord.
        }

        // Envoyer la commande au serveur.
        // Assurez-vous que le serveur attend un terminateur (comme \n) après chaque commande.
        // La méthode send() devrait gérer les erreurs réseau et lancer des exceptions si nécessaire.
        try {
            int cmdBytesSent = connection->send(command + "\n"); // Ajouter \n pour terminer la commande
            if (cmdBytesSent <= 0 && command != "QUIT") { // Log l'échec d'envoi, sauf si la commande est QUIT (où l'envoi peut échouer si le serveur a déjà fermé en réponse à QUIT)
                 LOG("Main_Cli ERROR : Échec de l'envoi de la commande ou serveur déconnecté pendant l'envoi. Code retour send: " + std::to_string(cmdBytesSent), "ERROR");
                 break; // Sort de la boucle en cas d'erreur d'envoi
            }
            // Log l'envoi, mais pas si la commande est QUIT (pour éviter double log avant la sortie)
            if (command != "QUIT") {
                LOG("Main_Cli DEBUG : Commande envoyée : '" + command + "'. (" + std::to_string(cmdBytesSent) + " bytes)", "DEBUG");
            }


        } catch (const std::exception& e) {
            LOG("Main_Cli ERROR : Exception lors de l'envoi de la commande '" + command + "'. Exception: " + std::string(e.what()), "ERROR");
            break; // Sort de la boucle en cas d'exception pendant l'envoi
        }


        // Si la commande était QUIT, on a envoyé le message, on sort maintenant de la boucle.
        if (command == "QUIT") {
            break; // Sort de la boucle de commandes
        }


        // Recevoir la réponse du serveur.
        // UTILISER receiveLine() pour lire un message complet terminé par \n.
        // receiveLine() gère l'accumulation de buffer et les erreurs.
        std::string serverResponse;
        try {
             serverResponse = connection->receiveLine(); // <-- Utilise receiveLine() !
        } catch (const std::exception& e) {
             LOG("Main_Cli ERROR : Échec de la réception de la réponse du serveur ou serveur déconnecté pendant la boucle de commande. Exception: " + std::string(e.what()), "ERROR");
             // receiveLine() loggue déjà la raison de l'échec (connexion fermée, etc.).
             break; // Sort de la boucle en cas d'erreur de réception
        }

        // Afficher la réponse du serveur à l'utilisateur
        std::cout << "< " << serverResponse << "\n";
        // Log la réponse reçue (peut être tronquée si très longue)
        LOG("Main_Cli DEBUG : Réponse serveur reçue (" + std::to_string(serverResponse.size()) + " bytes): '" + serverResponse.substr(0, std::min((size_t)serverResponse.size(), (size_t)200)) + ((serverResponse.size() > 200) ? "..." : "") + "'", "DEBUG");


    } // Fin de la boucle while(connection && connection->isConnected())

    LOG("Main_Cli INFO : Sortie de la boucle de commande.", "INFO");
    // La connexion sera fermée après la sortie de cette fonction, dans le main().
}


// --- Fonction main : Point d'entrée du programme client ---
int main(int argc, char* argv[]) {
    // Configuration initiale du Logger (si non fait dans son constructeur ou init)
    // Logger::init("client.log"); // Exemple: Initialise un logger fichier, sinon utilise cout/cerr par défaut

    LOG("Main_Cli INFO : Démarrage du programme client.", "INFO");

    // --- Parsing des arguments de ligne de commande ---
    // Le client attend 4 arguments SUPPLÉMENTAIRES après le nom de l'exécutable (argv[0]):
    // argv[1]: <server_host>
    // argv[2]: <server_port>
    // argv[3]: <client_id>
    // argv[4]: <client_token> (qui est le mot de passe en clair pour l'auth)
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <server_host> <server_port> <client_id> <client_token>\n";
        LOG("Main_Cli ERROR : Nombre d'arguments incorrect. Attendu 4 (host, port, id, token), reçu " + std::to_string(argc - 1) + ".", "ERROR");
        return EXIT_FAILURE;
    }

    std::string serverHost = argv[1];
    int serverPort = std::stoi(argv[2]); // Convertit le port de string en int
    std::string clientId = argv[3];
    std::string clientToken = argv[4]; // Ce sera le mot de passe en clair pour l'authentification


    LOG("Main_Cli INFO : Arguments parsés. Serveur: " + serverHost + ":" + std::to_string(serverPort) + ", Client ID: " + clientId + ", Token/Password: [CACHÉ]", "INFO");
    // Note : Ne jamais logguer le mot de passe en clair dans un système de production !

    // --- Initialisation OpenSSL (côté client) ---
    initialize_openssl_client(); // Appel de la fonction d'initialisation
    // Le nettoyage sera fait à la fin du main() via cleanup_openssl_client() ou dans le catch.


    // --- Création du contexte SSL client ---
    // Le contexte est nécessaire pour initier une connexion SSL.
    LOG("Main_Cli DEBUG : Création du contexte SSL client...", "DEBUG");
    ClientInitiator clientInitiator; // Crée l'objet ClientInitiator.
    // UniqueSSLCTX gère la libération du contexte à la sortie de portée.
    UniqueSSLCTX client_ctx = clientInitiator.InitClientCTX(); // Appelle la méthode pour créer le contexte.
    if (!client_ctx) {
        LOG("Main_Cli CRITICAL : Échec de la création du contexte SSL client. Arrêt.", "CRITICAL");
        cleanup_openssl_client(); // Nettoie OpenSSL globalement
        return EXIT_FAILURE;
    }
    LOG("Main_Cli DEBUG : Contexte SSL client créé avec succès.", "DEBUG");


    // --- Connexion au Serveur ---
    std::shared_ptr<ServerConnection> connection = nullptr; // Pointeur pour la connexion active.

    try {
        LOG("Main_Cli INFO : Tentative de connexion au serveur " + serverHost + ":" + std::to_string(serverPort) + "...", "INFO");
        // Utilise ClientInitiator pour connecter TCP et faire le handshake SSL.
        // Passe le pointeur brut (.get()) du contexte SSL client à la méthode.
        connection = clientInitiator.ConnectToServer(serverHost, serverPort, client_ctx.get());

        if (!connection || !connection->isConnected()) {
            // ConnectToServer loggue déjà les raisons de l'échec.
            LOG("Main_Cli CRITICAL : Échec de la connexion TCP ou du handshake SSL au serveur. Arrêt.", "CRITICAL");
            // Pas besoin de fermer 'connection', car si elle est nulle ou non connectée, il n'y a rien à fermer.
            // Le contexte client (client_ctx) sera automatiquement libéré ici par son destructeur UniqueSSLCTX.
            cleanup_openssl_client();
            return EXIT_FAILURE;
        }
        LOG("Main_Cli INFO : Connexion SSL au serveur établie. Socket FD: " + std::to_string(connection->getSocketFD()), "INFO");

        // --- Protocole d'Authentification (côté client) ---
        // Le client envoie le message au format "ID:votre_id,TOKEN:votre_mot_de_passe_en_clair"
        std::string authMessage = "ID:" + clientId + ",TOKEN:" + clientToken;
        LOG("Main_Cli DEBUG : Envoi message d'authentification...", "DEBUG");

        // Envoyer le message d'authentification suivi d'un newline.
        // La méthode send() devrait gérer les erreurs réseau et lancer des exceptions si nécessaire.
        try {
            int bytesSent = connection->send(authMessage + "\n"); // Ajouter \n
            if (bytesSent <= 0) {
                LOG("Main_Cli ERROR : Échec de l'envoi du message d'authentification. Code retour send: " + std::to_string(bytesSent), "ERROR");
                // Gérer l'échec d'envoi. closeConnection sera appelée dans le catch global ou à la sortie.
                throw std::runtime_error("Failed to send authentication message.");
            }
            LOG("Main_Cli DEBUG : Message d'authentification envoyé (" + std::to_string(bytesSent) + " bytes).", "DEBUG");

        } catch (const std::exception& e) {
             LOG("Main_Cli ERROR : Exception lors de l'envoi du message d'authentification. Exception: " + std::string(e.what()), "ERROR");
             // L'exception sera attrapée par le catch global pour un nettoyage.
             throw;
        }


        // Recevoir la réponse d'authentification du serveur ("AUTH SUCCESS", "AUTH NEW", "AUTH FAIL:...")
        // UTILISER receiveLine() pour lire la réponse complète terminée par newline.
        std::string authResponse;
        try {
            authResponse = connection->receiveLine(); // <-- Utilise receiveLine() pour la réponse Auth !
            // receiveLine() lancera une exception si la connexion est fermée ou s'il y a une erreur avant de recevoir une ligne complète.
        } catch (const std::exception& e) {
             LOG("Main_Cli ERROR : Échec de la réception de la réponse d'authentification ou serveur déconnecté. Exception: " + std::string(e.what()), "ERROR");
             // L'exception sera attrapée par le catch global pour un nettoyage.
             throw;
        }

        // Log la réponse authentification reçue.
        LOG("Main_Cli INFO : Réponse d'authentification reçue : '" + authResponse + "'", "INFO");

        // --- Vérifier la réponse d'authentification et agir en conséquence ---
        if (authResponse == "AUTH SUCCESS" || authResponse == "AUTH NEW") {
            // Authentification ou enregistrement et authentification réussis.
            LOG("Main_Cli INFO : Authentification/enregistrement réussi(e).", "INFO");
            // Le client est authentifié, on peut passer à la boucle de commandes interactive.

            // Lancer la boucle interactive de commandes.
            start_command_loop(connection); // Appel de la fonction qui gère l'invite et les commandes.

            // Lorsque start_command_loop se termine (par QUIT ou erreur réseau),
            // la connexion sera fermée après (voir code ci-dessous).

        } else if (authResponse.rfind("AUTH FAIL", 0) == 0) { // La réponse commence par "AUTH FAIL"
            // Authentification explicitement refusée par le serveur.
            LOG("Main_Cli WARNING : Authentification échouée. Message serveur : " + authResponse, "WARNING");
            std::cerr << "Échec de connexion. Réponse serveur : " << authResponse << std::endl;
            // Ne pas lancer la boucle de commandes. La connexion sera fermée ci-dessous.
            // Le programme principal sortira avec un code d'échec.

        } else {
            // Réponse d'authentification inattendue.
            LOG("Main_Cli ERROR : Réponse d'authentification inattendue du serveur : '" + authResponse + "'", "ERROR");
            std::cerr << "Échec de connexion. Réponse serveur inattendue : " << authResponse << std::endl;
            // Ne pas lancer la boucle de commandes. La connexion sera fermée ci-dessous.
            // Le programme principal sortira avec un code d'échec.
        }


    } catch (const std::exception& e) {
        // Gérer les exceptions non gérées pendant la connexion, l'authentification ou (si start_command_loop est inline) la boucle de commande.
        LOG("Main_Cli CRITICAL : Exception non gérée lors de la connexion ou de l'interaction avec le serveur. Exception: " + std::string(e.what()), "CRITICAL");
        std::cerr << "Erreur critique : " << e.what() << std::endl;

    } catch (...) {
        // Attraper toute autre exception inconnue.
        LOG("Main_Cli CRITICAL : Exception inconnue non gérée dans le programme client.", "CRITICAL");
        std::cerr << "Erreur critique inconnue." << std::endl;
    }

    // --- Fermeture de la connexion (si elle est encore active) ---
    // Cette étape s'exécute que la connexion ait réussi ou échoué, sauf si le programme a quitté plus tôt via return.
    // Si la connexion est valide et active (pointeur non null et isConnected() true), la fermer proprement.
    if (connection && connection->isConnected()) {
        LOG("Main_Cli INFO : Fermeture de la connexion client...", "INFO");
        try {
             connection->closeConnection(); // Assure la fermeture côté client (envoie shutdown SSL si possible).
             LOG("Main_Cli INFO : Connexion client fermée.", "INFO");
        } catch (const std::exception& e) {
             LOG("Main_Cli ERROR : Exception lors de la fermeture de la connexion client. Exception: " + std::string(e.what()), "ERROR");
        }
    } else {
         // Loguer si la connexion était déjà non valide/fermée au moment de la tentative de fermeture finale.
         LOG("Main_Cli DEBUG : Connexion client déjà non valide/fermée avant la tentative de fermeture finale.", "DEBUG");
    }


    // Le contexte client (client_ctx) est libéré automatiquement ici par son destructeur UniqueSSLCTX
    // (car client_ctx est une variable locale à main et sort de portée),
    // si le programme atteint la fin du main() sans quitter plus tôt via return avant la déclaration de client_ctx.
    // Si le programme quitte via return avant la déclaration de client_ctx, le cleanup OpenSSL global
    // est toujours appelé.

    // --- Nettoyage OpenSSL (côté client) ---
    // Ce cleanup est appelé en cas de succès ou d'échec géré avant la sortie du main().
    cleanup_openssl_client();


    LOG("Main_Cli INFO : Programme client terminé.", "INFO");

    // Retourner un code d'échec si une erreur s'est produite (si on est arrivé ici après un échec géré)
    // ou un code de succès. Une meilleure gestion des codes de retour des blocs try/catch serait nécessaire
    // pour différencier succès et échec à ce point. Pour l'instant, on retourne EXIT_SUCCESS si on arrive ici.
    return EXIT_SUCCESS; // Indique que le programme client s'est terminé "normalement" (même après un échec d'auth s'il a été géré).
}