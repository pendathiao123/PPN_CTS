#include "../headers/Client.h"
#include "../headers/Logger.h"  

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h> 
#include <openssl/crypto.h> 

#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <unistd.h>     
#include <netdb.h>      
#include <cstring>      
#include <cerrno>       

#include <iostream> 
#include <string>  
#include <memory>   
#include <stdexcept> 
#include <limits> 
#include <sstream> 
#include <cstdlib> 
#include <vector> 


int main(int argc, char const* argv[]) {

    // NOTE IMPORTANTE : Initialisation globale des bibliothèques (OpenSSL, Curl si utilisé)
    // Ceci doit être fait UNE SEULE FOIS au démarrage du programme client (ici dans main).
    // NE PAS appeler ces fonctions dans le constructeur ou une méthode de la classe Client.

    // Initialisation OpenSSL
    SSL_library_init(); // Initialise la bibliothèque SSL/TLS
    OpenSSL_add_all_algorithms(); // Charge tous les algorithmes (cipher, hash)
    SSL_load_error_strings(); // Remplacement moderne pour charger les chaînes d'erreur

    // Initialisation globale de libcurl si le CLIENT devait l'utiliser (peu probable pour la comm direct client-serveur)
    // curl_global_init(CURL_GLOBAL_DEFAULT);

    LOG("Client programme démarré.", "INFO");

    // Vérification des arguments de ligne de commande (adresse et port du serveur)
    if (argc != 3) {
        LOG("Usage: " + std::string(argv[0]) + " <server_address> <server_port>", "ERROR");
        // Nettoyer les bibliothèques globales avant de quitter en cas d'erreur
        EVP_cleanup();      // Nettoie les algorithmes EVP
        CRYPTO_cleanup_all_ex_data(); // Nettoie d'autres données OpenSSL
        return EXIT_FAILURE; // Quitter avec un code d'erreur
    }

    // Récupérer l'adresse et le port du serveur depuis les arguments
    std::string serverAddress = argv[1];
    int serverPort = 0;
    try {
        serverPort = std::stoi(argv[2]); // Convertir le port en entier
        if (serverPort <= 0 || serverPort > 65535) {
             throw std::out_of_range("Port number out of range.");
        }
    } catch (const std::exception& e) {
        LOG("Erreur: Port serveur invalide: " + std::string(argv[2]) + ". " + e.what(), "ERROR");
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data(); // Nettoyage global
        return EXIT_FAILURE;
    }


    // --- 1. Création du socket client ---
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0); // Crée un socket TCP/IPv4
    if (clientSocket < 0) {
        LOG("Erreur: Impossible de créer la socket. Erreur: " + std::string(strerror(errno)), "ERROR");
        // Nettoyer les bibliothèques globales et quitter
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data();
        return EXIT_FAILURE;
    }
    LOG("Socket client créé. FD: " + std::to_string(clientSocket), "DEBUG");


    // --- 2. Résolution de l'adresse du serveur et préparation de la structure d'adresse ---
    // Utilise getaddrinfo pour une résolution de nom de domaine plus moderne et flexible que gethostbyname (obsolète).
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Autorise IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM; // Socket de flux (TCP)

    int status = getaddrinfo(serverAddress.c_str(), argv[2], &hints, &res);
    if (status != 0) {
        LOG("Erreur: Impossible de résoudre l'adresse du serveur '" + serverAddress + "': " + std::string(gai_strerror(status)), "ERROR");
        close(clientSocket); // Ferme le socket créé
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data();
        return EXIT_FAILURE;
    }

    // Tenter de se connecter au premier résultat valide retourné par getaddrinfo
    int connect_status = -1;
    for(p = res; p != NULL; p = p->ai_next) {
        connect_status = connect(clientSocket, p->ai_addr, p->ai_addrlen);
        if (connect_status == 0) {
            // Connexion réussie
            break;
        }
    }

    freeaddrinfo(res); // Libère la structure allouée par getaddrinfo

    if (connect_status < 0) {
        LOG("Erreur: Connexion au serveur " + serverAddress + ":" + std::to_string(serverPort) + " échouée après toutes les tentatives.", "ERROR");
        close(clientSocket); // Ferme le socket
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data();
        return EXIT_FAILURE;
    }

    LOG("Connexion au serveur " + serverAddress + ":" + std::to_string(serverPort) + " établie (socket FD: " + std::to_string(clientSocket) + ").", "INFO");


    // --- 3. Initialisation SSL côté client ---
    // Crée un nouveau contexte SSL client. Utilise TLS_client_method() qui négocie TLSv1.0 à TLSv1.3.
    // Si tu utilises UniqueSSLCTX, crée-le ici : UniqueSSLCTX ctx_u(...);
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        LOG("Erreur: Impossible de créer le contexte SSL client.", "ERROR");
        ERR_print_errors_fp(stderr); // Log les erreurs OpenSSL détaillées sur stderr
        close(clientSocket); // Ferme le socket
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data();
        return EXIT_FAILURE;
    }
    
    // --- 4. Création de l'objet SSL et association avec le socket ---
    // Crée un nouvel objet SSL pour cette connexion.
    // Si tu utilises UniqueSSL, crée-le ici : UniqueSSL ssl_u(...);
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        LOG("Erreur: Impossible de créer l'objet SSL.", "ERROR");
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx); // Libère le contexte
        close(clientSocket); // Ferme le socket
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data();
        return EXIT_FAILURE;
    }

    // Associe l'objet SSL au descripteur de fichier du socket connecté
    SSL_set_fd(ssl, clientSocket);


    // --- 5. Effectuer le handshake SSL côté client ---
    // Ceci est une opération bloquante pour un socket bloquant.
    int ssl_connect_ret = SSL_connect(ssl);
    if (ssl_connect_ret <= 0) {
        int ssl_err = SSL_get_error(ssl, ssl_connect_ret); // Obtient le code d'erreur SSL détaillé
        LOG("Erreur: Échec du handshake SSL. Code SSL: " + std::to_string(ssl_err), "ERROR");
        ERR_print_errors_fp(stderr); // Log les erreurs OpenSSL détaillées
        SSL_free(ssl); // Libère l'objet SSL
        SSL_CTX_free(ctx); // Libère le contexte
        close(clientSocket); // Ferme le socket
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data();
        return EXIT_FAILURE;
    }

    LOG("Handshake SSL réussi.", "INFO");

    // --- La connexion SSL est maintenant prête pour la communication applicative ---

    // --- 6. Créer un objet Client pour gérer la communication de haut niveau ---
    // La classe Client encapsule le socket et l'objet SSL et gère les opérations d'envoi/réception via SSL_write/SSL_read.
    // L'objet Client prend possession du descripteur de fichier socket et du pointeur raw SSL* passés à son constructeur.
    // Son destructeur doit s'assurer de fermer le socket et de libérer l'objet SSL.
    auto client_comm = std::make_shared<Client>(clientSocket, ssl); // Client prend possession du socket et du SSL*

    // Note : À partir de maintenant, la fermeture du socket et la libération de l'objet SSL
    // sont gérées par le destructeur de l'objet Client lorsque le shared_ptr client_comm est détruit (sort de portée ou dernière référence disparaît).
    // Il ne faut plus appeler close(clientSocket) ou SSL_free(ssl) manuellement si client_comm est valide.


    // --- 7. Implémentation de la logique applicative du client ---
    // Ceci inclus l'authentification, l'envoi de requêtes, la réception et le traitement des réponses.

    // Étape d'authentification : Envoyer l'ID et le Token au serveur
    // --- Tâche future : Implémenter la logique pour gérer l'ID et le Token de manière persistante. ---
    // Dans un client réel, tu lirais l'ID et le token sauvegardés localement (par exemple, dans un fichier de configuration).
    // Si le fichier n'existe pas ou est vide, tu pourrais générer un nouvel ID/Token et les utiliser pour l'authentification "AUTH NEW".
    // Si le serveur répond "AUTH NEW" avec un ID/Token assigné, tu devrais sauvegarder ces valeurs assignées localement pour les prochaines connexions.
    // Utilise GenerateRandomId() et GenerateToken() si tu as implémenté la création automatique côté client aussi.

    std::string myClientId = "mon_client_id_unique"; // <<<--- REMPLACER CECI PAR LA LOGIQUE RÉELLE
    std::string myToken = "mon_token_secret";   // <<<--- REMPLACER CECI PAR LA LOGIQUE RÉELLE


    std::string authMessage = "ID:" + myClientId + ",TOKEN:" + myToken; // Message au format attendu par le serveur
    LOG("Envoi du message d'authentification : '" + authMessage + "'", "INFO");

    // Envoyer le message d'authentification via l'objet Client
    if (!client_comm->send(authMessage.c_str(), authMessage.size())) {
        LOG("Erreur lors de l'envoi du message d'authentification.", "ERROR");
        // La connexion sera fermée par le destructeur de client_comm.
        SSL_CTX_free(ctx); // Libère le contexte SSL (créé manuellement)
        EVP_cleanup(); CRYPTO_cleanup_all_ex_data(); // Nettoyage global
        return EXIT_FAILURE;
    }

    // Recevoir la réponse d'authentification du serveur
    char authResponseBuffer[1024]; // Buffer pour la réponse d'authentification (taille raisonnable)
    // client_comm->receive retourne le nombre d'octets lus, 0 pour déconnexion propre, <0 pour erreur.
    int bytesRead = client_comm->receive(authResponseBuffer, sizeof(authResponseBuffer) - 1); // Attente bloquante
    if (bytesRead <= 0) {
         // Gérer la déconnexion propre (0) ou les erreurs de réception (<0).
         LOG("Erreur (" + std::to_string(bytesRead) + ") ou déconnexion du serveur lors de la réception de la réponse d'authentification.", "ERROR");
         // La connexion sera fermée par le destructeur de client_comm.
         SSL_CTX_free(ctx); // Libère le contexte SSL (créé manuellement)
         EVP_cleanup(); CRYPTO_cleanup_all_ex_data(); // Nettoyage global
         return EXIT_FAILURE;
    }
    authResponseBuffer[bytesRead] = '\0'; // Null-terminer la chaîne reçue
    std::string authResponse(authResponseBuffer); // Convertir le buffer en std::string
    LOG("Réponse d'authentification reçue : '" + authResponse + "'", "INFO");

    // Vérifier la réponse d'authentification du serveur
    if (authResponse.rfind("AUTH FAIL", 0) == 0) { // rfind avec 0 pour vérifier si la chaîne commence par "AUTH FAIL"
         LOG("Authentification échouée. Vérifiez votre ID et Token. Fermeture de la connexion.", "ERROR");
         // La connexion sera fermée par le destructeur de client_comm.
         SSL_CTX_free(ctx); // Libère le contexte SSL (créé manuellement)
         EVP_cleanup(); CRYPTO_cleanup_all_ex_data(); // Nettoyage global
         return EXIT_FAILURE; // Quitter avec un code d'erreur
    }
    // Si l'authentification réussit (AUTH SUCCESS ou AUTH NEW)
    if (authResponse.rfind("AUTH SUCCESS", 0) == 0 || authResponse.rfind("AUTH NEW", 0) == 0) {
         LOG("Authentification réussie ! Client prêt pour les commandes applicatives.", "INFO");
         // Si un nouveau compte a été créé par le serveur (AUTH NEW), extraire l'ID/Token et les sauvegarder.
         if (authResponse.rfind("AUTH NEW", 0) == 0) {
              // Exemple de parsing simple pour extraire ID et Token assignés par le serveur après "AUTH NEW:"
              // Le format est "AUTH NEW:assignedId,assignedToken"
              size_t colon_pos = authResponse.find(":", strlen("AUTH NEW")); // Cherche le premier ':' après "AUTH NEW"
              size_t comma_pos = authResponse.find(",", colon_pos); // Cherche la première ',' après ce ':'
              if (colon_pos != std::string::npos && comma_pos != std::string::npos && comma_pos > colon_pos) {
                   std::string assignedId = authResponse.substr(colon_pos + 1, comma_pos - (colon_pos + 1));
                   std::string assignedToken = authResponse.substr(comma_pos + 1); // Reste de la chaîne après ','
                   LOG("Serveur a créé un nouveau compte. ID assigné : " + assignedId + ", Token assigné : " + assignedToken, "INFO");
                   // --- Tâche importante : sauvegarder assignedId et assignedToken dans un fichier de configuration local. ---
                   // Exemple : saveConfig("client_config.txt", assignedId, assignedToken);
                   // Ces valeurs devraient ensuite être lues au prochain démarrage du client.
               } else {
                   LOG("Avertissement : Impossible de parser l'ID et le Token de la réponse AUTH NEW : '" + authResponse + "'", "WARNING");
               }
         }

        // --- 8. Début de la boucle interactive pour envoyer des commandes au serveur ---
        // Le client est maintenant authentifié et prêt à envoyer/recevoir des commandes applicatives.
        std::string userInput; // Pour stocker l'entrée de l'utilisateur
        char serverResponseBuffer[4096]; // Buffer pour les réponses du serveur (taille ajustée pour historique/wallet)

        std::cout << "Authentification réussie. Connecté au serveur. Entrez vos commandes (tapez 'EXIT' ou 'exit' pour quitter) :" << std::endl;

        // Boucle principale pour lire l'entrée de l'utilisateur et communiquer avec le serveur
        while (true) { // La boucle continue tant qu'on n'envoie pas 'EXIT' ou qu'il n'y a pas d'erreur critique de communication.
            std::cout << "> "; // Invite de commande pour l'utilisateur

            // Lire une ligne d'entrée depuis la console (jusqu'au saut de ligne)
            // std::cin >> peut être problématique avec des lignes contenant des espaces ou après des erreurs.
            // std::getline est plus sûr pour lire une ligne complète.
            std::getline(std::cin, userInput);

            // Gérer les cas où la lecture échoue (ex: fin de fichier, erreur irrécupérable sur cin)
            if (std::cin.fail()) {
                 LOG("Erreur lors de la lecture de l'entrée utilisateur. Sortie de la boucle interactive.", "ERROR");
                 break; // Sortir de la boucle interactive
            }
            if (std::cin.eof()) {
                 LOG("Fin de l'entrée utilisateur (EOF). Sortie de la boucle interactive.", "INFO");
                 break; // Sortir de la boucle interactive
            }

            // Si l'utilisateur tape 'EXIT' (ou 'exit'), envoyer cette commande au serveur et quitter la boucle côté client.
            // Le serveur est configuré pour arrêter la session sur réception de "EXIT".
            if (userInput == "EXIT" || userInput == "exit") { // Permettre 'exit' aussi
                 LOG("Commande 'EXIT' ou 'exit' saisie. Envoi au serveur et fermeture du client.", "INFO");
                 // Envoyer la commande EXIT au serveur. L'échec d'envoi ici n'est pas critique, on sort quand même.
                 client_comm->send(userInput.c_str(), userInput.size()); // Tentative d'envoi
                 // Sortir de la boucle interactive
                 break;
            }

            // Si l'entrée n'est pas vide (et n'est pas la commande de sortie)
            if (!userInput.empty()) {
                 // Envoyer l'entrée utilisateur comme requête au serveur
                 LOG("Envoi de la requête: '" + userInput + "'", "INFO");
                 if (!client_comm->send(userInput.c_str(), userInput.size())) {
                     LOG("Erreur lors de l'envoi de la requête '" + userInput + "'. Déconnexion ou erreur réseau probable.", "ERROR");
                     // Si l'envoi échoue, la connexion est probablement cassée. Sortir de la boucle.
                     break;
                 }

                 // Recevoir la réponse du serveur
                 // Le buffer doit être assez grand pour la réponse attendue (portefeuille, historique).
                 // client_comm->receive retourne le nombre d'octets lus, 0 pour déconnexion propre, <0 pour erreur.
                 bytesRead = client_comm->receive(serverResponseBuffer, sizeof(serverResponseBuffer) - 1);
                 if (bytesRead <= 0) {
                      // Gérer la déconnexion propre (0) ou les erreurs de réception (<0).
                      LOG("Erreur (" + std::to_string(bytesRead) + ") ou déconnexion du serveur lors de la réception de la réponse.", "ERROR");
                      // Si la réception échoue ou si le serveur déconnecte, sortir de la boucle.
                      break;
                 }

                 serverResponseBuffer[bytesRead] = '\0'; // Null-terminer la chaîne reçue
                 std::string serverResponse(serverResponseBuffer); // Convertir le buffer en std::string
                 LOG("Réponse du serveur :\n" + serverResponse, "INFO"); // Afficher la réponse reçue
            } // Fin if (!userInput.empty())

        } // --- Fin de la boucle while (true) interactive ---

        // Si on sort de la boucle interactive (par commande EXIT saisie, erreur send/receive, ou erreur cin/eof)
        LOG("Boucle interactive terminée. Fermeture de la connexion et nettoyage.", "INFO");


    } else { // Bloc pour gérer les réponses d'authentification autres que SUCCESS/NEW/FAIL (imprévues)
         // Réponse d'authentification inattendue du serveur.
         LOG("Réponse d'authentification inattendue du serveur : '" + authResponse + "'. Fermeture.", "WARNING");
         // Le destructeur de client_comm gérera la fermeture de la connexion.
         SSL_CTX_free(ctx); // Libère le contexte SSL (créé manuellement)
         return EXIT_FAILURE; // Quitter avec un code d'erreur
    }

    // --- La connexion sera fermée lorsque l'objet client_comm (shared_ptr) sera détruit ---
    // (lorsqu'il sortira de sa portée à la fin de main()).
    // Le contexte SSL ctx a été libéré ci-dessus après la boucle interactive (si authentification réussie)
    // ou dans les branches d'erreur d'authentification/handshake/connexion.
    // Si UniqueSSLCTX était utilisé, sa destruction automatique suffirait ici.


    // --- Nettoyage global des bibliothèques ---
    // Ces fonctions doivent être appelées UNE SEULE FOIS à la fin du programme client,
    // après que toutes les opérations SSL/socket soient terminées et tous les objets SSL/SSL_CTX libérés.
    EVP_cleanup();      // Nettoie les algorithmes EVP (si utilisés, bonne pratique)
    CRYPTO_cleanup_all_ex_data(); // Nettoie d'autres données OpenSSL

    LOG("Client programme terminé.", "INFO");

    return EXIT_SUCCESS; // Quitter avec succès
}