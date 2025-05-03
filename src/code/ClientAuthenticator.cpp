#include "ClientAuthenticator.h"
#include "Logger.h"
#include "ServerConnection.h"
#include "Server.h"
#include "Utils.h"


#include <iostream>      
#include <sstream>       
#include <string>        
#include <limits>        
#include <cstring>       
#include <openssl/err.h> 
#include <cerrno>        
#include <algorithm>     
#include <cctype>        


// --- Implémentation de la fonction utilitaire authOutcomeToString ---
std::string authOutcomeToString(AuthOutcome outcome) {
    switch (outcome) {
        case AuthOutcome::FAIL:    return "FAIL";
        case AuthOutcome::SUCCESS: return "SUCCESS";
        case AuthOutcome::NEW:     return "NEW";
        case AuthOutcome::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN"; // Retour par défaut pour robustesse.
    }
}


// --- Implémentation du Constructeur ClientAuthenticator ---
ClientAuthenticator::ClientAuthenticator() {}


AuthOutcome ClientAuthenticator::AuthenticateClient(ServerConnection& client_conn, Server& server_instance, std::string& authenticatedUserId) {
    LOG("ClientAuthenticator::AuthenticateClient INFO : Démarrage authentification pour socket FD: " + std::to_string(client_conn.getSocketFD()), "INFO");

    // Déclarer ces variables pour le parsing.
    std::string receivedId;
    std::string receivedPassword;

    // --- 1. Recevoir le message d'authentification du client ---
    std::string authMessage;
    try {
        // receiveLine() lit jusqu'au \n et gère l'accumulation de buffer/erreurs.
        authMessage = client_conn.receiveLine();
        // receiveLine lancera une exception en cas d'échec grave ou de déconnexion.
    } catch (const std::exception& e) {
        // Gérer l'échec de lecture de ligne (déconnexion, erreur).
        LOG("ClientAuthenticator::AuthenticateClient ERROR : Échec ou déconnexion lors de la réception du message d'auth pour socket FD: " + std::to_string(client_conn.getSocketFD()) + ". Exception: " + e.what(), "ERROR");
        // Retourner FAIL. HandleClient fermera la connexion si nécessaire.
        return AuthOutcome::FAIL;
    }

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
                    // NE PAS ENVOYER NI FERMER. Retourner FAIL. Signaler l'échec du parsing.
                    return AuthOutcome::FAIL;
                }

            } else {
                LOG("ClientAuthenticator::AuthenticateClient WARNING : Format message invalide (TOKEN manquant/incorrect) : '" + authMessage + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()), "WARNING");
                // NE PAS ENVOYER NI FERMER. Retourner FAIL. Signaler l'échec du parsing.
                return AuthOutcome::FAIL;
            }
        } else {
            LOG("ClientAuthenticator::AuthenticateClient WARNING : Format message invalide (ID manquant/mal formaté) : '" + authMessage + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()), "WARNING");
            // NE PAS ENVOYER NI FERMER. Retourner FAIL. Signaler l'échec du parsing.
            return AuthOutcome::FAIL;
        }
    } else {
         LOG("ClientAuthenticator::AuthenticateClient WARNING : Format message invalide (ID manquant/incorrect) : '" + authMessage + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()), "WARNING");
         // NE PAS ENVOYER NI FERMER. Retourner FAIL. Signaler l'échec du parsing.
         return AuthOutcome::FAIL;
    }


    // --- 3. Demander au Server de vérifier ou enregistrer l'utilisateur ---
    // La logique de vérification/enregistrement est dans Server::processAuthRequest.
    // AuthenticatedUserId est un paramètre OUT que le Server remplit si succès.
    // On ne passe PAS reasonForFailure ici, car HandleClient gère les messages d'échec génériques.
    AuthOutcome authResultFromServer = server_instance.processAuthRequest(receivedId, receivedPassword, authenticatedUserId);


    // --- 4. Retourner le résultat au Server::HandleClient ---
    // C'est HandleClient qui décidera quoi envoyer au client (SUCCESS/NEW ou FAIL)
    // basé sur ce résultat ET la vérification activeSessions. HandleClient gère aussi la fermeture.

    if (authResultFromServer == AuthOutcome::SUCCESS || authResultFromServer == AuthOutcome::NEW) {
        // Authentification réussie selon Server::processAuthRequest.
        LOG("ClientAuthenticator::AuthenticateClient INFO : Authentification terminée avec succès pour ID: '" + authenticatedUserId + "'. Résultat: " + authOutcomeToString(authResultFromServer) + ". Socket FD: " + std::to_string(client_conn.getSocketFD()), "INFO");
        // authenticatedUserId a été rempli par processAuthRequest. Il est retourné via le paramètre OUT.
        // NE PAS ENVOYER DE RÉPONSE NI FERMER LA CONNEXION ICI. Retourne le résultat (SUCCESS ou NEW).
        return authResultFromServer;

    } else { // authResultFromServer == AuthOutcome::FAIL
        // L'authentification a échoué selon Server::processAuthRequest (identifiants invalides, etc.).
        LOG("ClientAuthenticator::AuthenticateClient INFO : Authentification échouée pour ID: '" + receivedId + "'. Socket FD: " + std::to_string(client_conn.getSocketFD()) + ". Server a retourné FAIL. Connexion non fermée par Authenticator.", "INFO");
        // authenticatedUserId n'est PAS valide ici.
        // NE PAS ENVOYER DE RÉPONSE NI FERMER LA CONNEXION ICI. HandleClient gérera l'envoi de AUTH FAIL et la fermeture. Retourne le résultat Échec.
        return AuthOutcome::FAIL;
    }
}