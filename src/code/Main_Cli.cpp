#include <iostream>      
#include <string>       
#include <vector>        
#include <stdexcept>    
#include <memory>     
#include <limits>     
#include <cstdlib>      
#include <cstring>      
#include <algorithm>   
#include <cerrno>       

#include <openssl/ssl.h>   
#include <openssl/err.h>   
#include <openssl/rand.h>

#include "../headers/ClientInitiator.h" 
#include "../headers/ServerConnection.h" 
#include "../headers/Logger.h"           
#include "../headers/Utils.h"            
#include "../headers/Transaction.h"      
#include "../headers/OpenSSLDeleters.h" 


// Constantes et macros
#define RECEIVE_BUFFER_SIZE 4096 // Taille du buffer pour receiveLine (peut être interne à ServerConnection)

// --- Initialisation globale OpenSSL (côté client) ---
void initialize_openssl_client() {
    // Load encryption & hashing algorithms
    OpenSSL_add_all_algorithms();
    // Load SSL/TLS algorithms
    SSL_library_init(); // Obsolète dans les versions très récentes, mais compatible
    // Load error strings
    SSL_load_error_strings();
}

// --- Nettoyage global OpenSSL (côté client) ---
void cleanup_openssl_client() {
    LOG("Main_Cli INFO : Nettoyage global OpenSSL client...", "INFO");
    EVP_cleanup();
    // CRYPTO_cleanup_all_ex_data(); // Libère les données d'index allouées par CRYPTO_get_ex_new_index - Souvent pas nécessaire avec des versions récentes
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
    while (connection && connection->isConnected()) {
        std::cout << "> "; // Invite de commande
        if (!std::getline(std::cin, command)) {
            LOG("Main_Cli INFO : Échec de lecture de la commande (stdin) ou fin de fichier. Déconnexion.", "INFO");
            break;
        }

        command.erase(0, command.find_first_not_of(" \t\n\r\f\v"));
        command.erase(command.find_last_not_of(" \t\n\r\f\v") + 1);

        if (command.empty()) {
            continue;
        }

        bool is_trading_command = (command.rfind("BUY", 0) == 0 || command.rfind("SELL", 0) == 0); // Vérifie si c'est une commande de trading
        bool is_quit_command = (command == "QUIT");

        // Envoyer la commande au serveur.
        try {
            int cmdBytesSent = connection->send(command + "\n");
            if (cmdBytesSent <= 0 && !is_quit_command) {
                 LOG("Main_Cli ERROR : Échec de l'envoi de la commande ou serveur déconnecté pendant l'envoi. Code retour send: " + std::to_string(cmdBytesSent), "ERROR");
                 break;
            }

        } catch (const std::exception& e) {
            LOG("Main_Cli ERROR : Exception lors de l'envoi de la commande '" + command + "'. Exception: " + std::string(e.what()), "ERROR");
            break;
        }

        if (is_quit_command) {
            break; // Sort de la boucle de commandes si c'était QUIT
        }

        // --- Section de Réception des Réponses ---
        std::string serverResponse;
        [[maybe_unused]] bool received_ok_for_trading_command = false; // Flag pour savoir si on a reçu l'accusé "OK: ..." pour un trade.

        while (connection && connection->isConnected()) { // Boucle de réception
            try {
                 serverResponse = connection->receiveLine(); // Utilise receiveLine()
            } catch (const std::exception& e) {
                 LOG("Main_Cli ERROR : Échec de la réception de la réponse du serveur ou serveur déconnecté pendant la boucle de commande. Exception: " + std::string(e.what()), "ERROR");
                 // Si receiveLine échoue (ex: connexion fermée), on sort de la boucle de réception ET de la boucle principale.
                 goto exit_reception_loop; // Utilisation d'un goto pour sortir proprement des boucles imbriquées
            }

            // Afficher la réponse reçue
            std::cout << "< " << serverResponse << "\n";

            // --- Logique pour arrêter la réception ---

            // Cas 1 : La commande n'était pas de trading. On attend une seule réponse.
            if (!is_trading_command) {
                break; // Sort de la boucle de réception après la première ligne
            }

            // Cas 2 : La commande était de trading (BUY/SELL).
            // On a reçu une réponse.
            // Vérifier si cette réponse est l'accusé de réception "OK: ..."
            if (serverResponse.rfind("OK:", 0) == 0) {
                // C'est l'accusé de réception initial pour une commande de trading.
                received_ok_for_trading_command = true;
                // On NE break PAS ici. On continue la boucle de réception pour attendre TRANSACTION_RESULT.

            }
            // Vérifier si cette réponse est le résultat final "TRANSACTION_RESULT..."
            else if (serverResponse.rfind("TRANSACTION_RESULT", 0) == 0) {
                // C'est le message final attendu pour une commande de trading.
                break; // Sort de la boucle de réception après le message final
            }
            // Si ce n'est NI "OK:..." NI "TRANSACTION_RESULT...",
            // c'est probablement un message d'erreur (comme "ERROR: Manual trading...")
            // ou un message inattendu. Dans ce cas, cette réponse est la réponse finale.
            else {
                 LOG("Main_Cli WARNING : Reçu réponse inattendue ou message d'erreur pour commande de trading. Traité comme réponse finale.", "WARNING");
                break; // Sort de la boucle de réception car la réponse est finale (erreur ou autre)
            }

            // Si on arrive ici dans la boucle de réception d'une commande de trading,
            // c'est qu'on a reçu "OK:..." et qu'on continue la boucle pour attendre TRANSACTION_RESULT.

        } // Fin de la boucle de réception while

    // Label pour le goto en cas d'erreur de réception
    exit_reception_loop:;


        // Si la boucle de réception s'est terminée à cause d'une erreur (goto),
        // la boucle principale se terminera aussi.
        if (!connection || !connection->isConnected()) {
             break; // Sort de la boucle principale
        }

        // Si on arrive ici, la boucle de réception s'est terminée normalement (via break).
        // La boucle principale while(connection && connection->isConnected()) continue
        // et retournera au début pour afficher "> " et lire la prochaine commande.


    } // Fin de la boucle while(connection && connection->isConnected()) principale

    LOG("Main_Cli INFO : Sortie de la boucle de commande.", "INFO");
    // La connexion sera fermée après la sortie de cette fonction, dans le main().
}

// --- Fonction main : Point d'entrée du programme client ---
int main(int argc, char* argv[]) {
    // Configuration initiale du Logger (si non fait dans son constructeur ou init)

    LOG("Main_Cli INFO : Démarrage du programme client.", "INFO");

    // --- Parsing des arguments de ligne de commande ---
    // Le client attend 4 arguments SUPPLÉMENTAIRES après le nom de l'exécutable (argv[0]):
    // <server_host> <server_port> <client_id> <client_token>
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
    ClientInitiator clientInitiator; // Crée l'objet ClientInitiator.
    // UniqueSSLCTX gère la libération du contexte à la sortie de portée.
    UniqueSSLCTX client_ctx = clientInitiator.InitClientCTX(); // Appelle la méthode pour créer le contexte.
    if (!client_ctx) {
        LOG("Main_Cli CRITICAL : Échec de la création du contexte SSL client. Arrêt.", "CRITICAL");
        cleanup_openssl_client(); // Nettoie OpenSSL globalement
        return EXIT_FAILURE;
    }


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

        // Envoyer le message d'authentification suivi d'un newline.
        // La méthode send() devrait gérer les erreurs réseau et lancer des exceptions si nécessaire.
        try {
            int bytesSent = connection->send(authMessage + "\n");
            if (bytesSent <= 0) {
                LOG("Main_Cli ERROR : Échec de l'envoi du message d'authentification. Code retour send: " + std::to_string(bytesSent), "ERROR");
                // Gérer l'échec d'envoi.
                throw std::runtime_error("Failed to send authentication message.");
            }
        } catch (const std::exception& e) {
             LOG("Main_Cli ERROR : Exception lors de l'envoi du message d'authentification. Exception: " + std::string(e.what()), "ERROR");
             // L'exception sera attrapée par le catch global pour un nettoyage.
             throw;
        }


        // Recevoir la réponse d'authentification du serveur ("AUTH SUCCESS", "AUTH NEW", "AUTH FAIL:...")
        // Utilise receiveLine() pour lire la réponse complète terminée par newline.
        std::string authResponse;
        try {
            authResponse = connection->receiveLine(); // Utilise receiveLine() pour la réponse Auth !
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
        // Gérer les exceptions non gérées pendant la connexion, l'authentification ou la boucle de commande.
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
        
    }


    // Le contexte client (client_ctx) est libéré automatiquement ici par son destructeur UniqueSSLCTX
    // (car client_ctx est une variable locale à main et sort de portée).

    // --- Nettoyage OpenSSL (côté client) ---
    // Ce cleanup est appelé en cas de succès ou d'échec géré avant la sortie du main().
    cleanup_openssl_client();


    LOG("Main_Cli INFO : Programme client terminé.", "INFO");

    // Retourner un code d'échec si une erreur s'est produite (si on est arrivé ici après un échec géré)
    // ou un code de succès. Une meilleure gestion des codes de retour des blocs try/catch serait nécessaire
    // pour différencier succès et échec à ce point. Pour l'instant, on retourne EXIT_SUCCESS si on arrive ici.
    return EXIT_SUCCESS; // Indique que le programme client s'est terminé "normalement" (même après un échec d'auth s'il a été géré).
}