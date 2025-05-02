#include "ClientAuthenticator.h"
#include "Logger.h"
#include "ServerConnection.h"
#include "Server.h"
#include "Utils.h"

// Includes pour les fonctionnalités standards et systèmes utilisées.
#include <iostream>      // std::cerr, std::endl
#include <sstream>       // std::stringstream, std::istringstream
#include <string>        // std::string, std::getline
#include <limits>        // std::numeric_limits
#include <cstring>       // strlen, strerror
#include <openssl/err.h> // ERR_get_error, ERR_error_string_n (si utilisé directement pour log SSL)
#include <cerrno>        // errno
#include <algorithm>     // std::transform (si utilisé pour parsing)
#include <cctype>        // ::tolower (si utilisé pour parsing)


// --- Implémentation de la fonction utilitaire authOutcomeToString ---
std::string authOutcomeToString(AuthOutcome outcome) {
    switch (outcome) {
        case AuthOutcome::FAIL:    return "FAIL";
        case AuthOutcome::SUCCESS: return "SUCCESS";
        case AuthOutcome::NEW:     return "NEW";
        case AuthOutcome::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}


// --- Implémentation du Constructeur ClientAuthenticator ---
ClientAuthenticator::ClientAuthenticator() {
    LOG("ClientAuthenticator::ClientAuthenticator DEBUG : Objet ClientAuthenticator créé.", "DEBUG");
}


AuthOutcome ClientAuthenticator::AuthenticateClient(ServerConnection& client_conn, Server& server_instance, std::string& authenticatedUserId) {
    LOG("ClientAuthenticator::AuthenticateClient INFO : Démarrage authentification pour socket FD: " + std::to_string(client_conn.getSocketFD()), "INFO");

    // Déclarer ces variables pour le parsing.
    std::string receivedId;
    std::string receivedPassword;

    // --- 1. Recevoir le message d'authentification du client (UTILISER receiveLine) ---
    std::string authMessage;
    try {
        // receiveLine() lit jusqu'au \n et gère l'accumulation de buffer/erreurs.
        authMessage = client_conn.receiveLine(); // <-- Utiliser receiveLine() ici !
        // receiveLine lancera une exception en cas d'échec grave ou de déconnexion.
    } catch (const std::exception& e) {
        // Gérer l'échec de lecture de ligne (déconnexion, erreur).
        LOG("ClientAuthenticator::AuthenticateClient ERROR : Échec ou déconnexion lors de la réception du message d'auth pour socket FD: " + std::to_string(client_conn.getSocketFD()) + ". Exception: " + e.what(), "ERROR");
        // Retourner FAIL. HandleClient fermera la connexion si nécessaire.
        return AuthOutcome::FAIL;
    }


    LOG("ClientAuthenticator::AuthenticateClient DEBUG : Reçu message d'auth de socket FD " + std::to_string(client_conn.getSocketFD()) + " : '" + authMessage + "'", "DEBUG");

    // --- 2. Parser le message d'authentification (Format attendu : "ID:votre_id,TOKEN:votre_mot_de_passe_en_clair") ---
    std::istringstream iss(authMessage);
    std::string part;

    // Tente d'extraire "ID" puis l'ID, puis "TOKEN" puis le mot de passe.
    if (std::getline(iss, part, ':') && part == "ID") {
        if (std::getline(iss, receivedId, ',')) {
            if (std::getline(iss, part, ':') && part == "TOKEN") {
                std::getline(iss, receivedPassword);

                // Nettoyer les espaces blancs.
                receivedId.erase(0, receivedId.find_first_not_of(" \t\n\r\f\v"));
                receivedId.erase(receivedId.find_last_not_of(" \t\n\r\f\v") + 1);
                receivedPassword.erase(0, receivedPassword.find_first_not_of(" \t\n\r\f\v"));
                receivedPassword.erase(receivedPassword.find_last_not_of(" \t\n\r\f\v") + 1);

                // Vérification basique de l'ID non vide.
                if (receivedId.empty()) {
                    LOG("ClientAuthenticator::AuthenticateClient WARNING : ID client vide après parsing. Socket FD: " + std::to_string(client_conn.getSocketFD()), "WARNING");
                    // NE PAS ENVOYER NI FERMER. Retourner FAIL.
                    return AuthOutcome::FAIL; // Signaler l'échec du parsing.
                }

                 // Ne logguer JAMAIS le mot de passe en clair dans un système de production !
                 LOG("ClientAuthenticator::AuthenticateClient DEBUG : Parsing réussi. ID: '" + receivedId + "', Mot de passe: '[CACHÉ]'. Socket FD: " + std::to_string(client_conn.getSocketFD()), "DEBUG");

            } else {
                LOG("ClientAuthenticator::AuthenticateClient WARNING : Format message invalide (TOKEN manquant/incorrect) : '" + authMessage + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()), "WARNING");
                // NE PAS ENVOYER NI FERMER. Retourner FAIL.
                return AuthOutcome::FAIL; // Signaler l'échec du parsing.
            }
        } else {
            LOG("ClientAuthenticator::AuthenticateClient WARNING : Format message invalide (ID manquant/mal formaté) : '" + authMessage + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()), "WARNING");
            // NE PAS ENVOYER NI FERMER. Retourner FAIL.
            return AuthOutcome::FAIL; // Signaler l'échec du parsing.
        }
    } else {
         LOG("ClientAuthenticator::AuthenticateClient WARNING : Format message invalide (ID manquant/incorrect) : '" + authMessage + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()), "WARNING");
         // NE PAS ENVOYER NI FERMER. Retourner FAIL.
         return AuthOutcome::FAIL; // Signaler l'échec du parsing.
    }


    // --- 3. Demander au Server de vérifier ou enregistrer l'utilisateur ---
    // La logique de vérification/enregistrement est dans Server::processAuthRequest.
    // AuthenticatedUserId est un paramètre OUT que le Server remplit si succès.
    // On ne passe PAS reasonForFailure ici, car HandleClient gère les messages d'échec génériques
    // ou le message "Already connected".
    LOG("ClientAuthenticator::AuthenticateClient DEBUG : Demande au Server de traiter la requête d'auth pour ID: " + receivedId + "...", "DEBUG");
    // Assumer que processAuthRequest prend ces 3 arguments (ID, Password, OUT authenticatedUserId).
    AuthOutcome authResultFromServer = server_instance.processAuthRequest(receivedId, receivedPassword, authenticatedUserId); // Signature attendue : (const std::string&, const std::string&, std::string&)


    // --- 4. Retourner le résultat au Server::HandleClient ---
    // C'est HandleClient qui décidera quoi envoyer au client (SUCCESS/NEW ou FAIL)
    // basé sur ce résultat ET la vérification activeSessions. HandleClient gère aussi la fermeture.

    if (authResultFromServer == AuthOutcome::SUCCESS || authResultFromServer == AuthOutcome::NEW) {
        // Authentification réussie selon Server::processAuthRequest.
        LOG("ClientAuthenticator::AuthenticateClient INFO : Authentification terminée avec succès pour ID: '" + authenticatedUserId + "'. Résultat: " + authOutcomeToString(authResultFromServer) + ". Socket FD: " + std::to_string(client_conn.getSocketFD()), "INFO");
        // authenticatedUserId a été rempli par processAuthRequest. Il est retourné via le paramètre OUT.
        // NE PAS ENVOYER DE RÉPONSE NI FERMER LA CONNEXION ICI.
        return authResultFromServer; // Retourne le résultat (SUCCESS ou NEW).

    } else { // authResultFromServer == AuthOutcome::FAIL
        // L'authentification a échoué selon Server::processAuthRequest (identifiants invalides, etc.).
        LOG("ClientAuthenticator::AuthenticateClient INFO : Authentification échouée pour ID: '" + receivedId + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()) + ". Server a retourné FAIL. Connexion non fermée par Authenticator.", "INFO");
        // authenticatedUserId n'est PAS valide ici.
        // NE PAS ENVOYER DE RÉPONSE NI FERMER LA CONNEXION ICI. HandleClient gérera l'envoi de AUTH FAIL et la fermeture.
        return AuthOutcome::FAIL; // Retourne le résultat Échec.
    }

    // Cette partie du code ne devrait jamais être atteinte. Ajouter pour être complet si nécessaire.
    // return AuthOutcome::FAIL; // Retour par défaut si on arrive ici (ne devrait pas).
}