#ifndef CLIENT_AUTHENTICATOR_H
#define CLIENT_AUTHENTICATOR_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <vector>

// Inclusion car ServerConnection est utilisé dans la signature de AuthenticateClient
#include "../headers/ServerConnection.h"


// Forward declaration
class Server;


// --- Énumération AuthOutcome : Résultat d'une tentative d'authentification ---
enum class AuthOutcome {
    FAIL,    // Échec
    SUCCESS, // Authentification réussie (utilisateur existant)
    NEW,     // Authentification réussie (nouvel utilisateur créé)
    UNKNOWN  // État inconnu ou erreur
};

// --- Déclaration de la fonction utilitaire pour convertir l'enum en string ---
std::string authOutcomeToString(AuthOutcome outcome);


// --- Classe ClientAuthenticator ---
// Gère la réception et le parsing du message d'authentification client.
// Cette classe se concentre sur l'interaction réseau initiale pour obtenir les identifiants.
// La vérification et l'enregistrement des utilisateurs sont délégués au Server.
class ClientAuthenticator {
public:
    // Constructeur par défaut.
    ClientAuthenticator();

    // Reçoit le message d'authentification, le parse, et demande au Server de vérifier/enregistrer l'utilisateur.
    // Interagit avec le client via client_conn (pour receive/send et stockage ID/Token en cas de succès).
    // Appelle les méthodes du Server (server_instance) pour la logique utilisateur.
    // Gère l'envoi des messages d'échec et la fermeture de la connexion en cas d'échec.
    // Retourne le résultat de l'authentification (AuthOutcome::SUCCESS, AuthOutcome::NEW, AuthOutcome::FAIL).
    AuthOutcome AuthenticateClient(ServerConnection& client_conn, Server& server_instance, std::string& authenticatedUserId);

}; 

#endif 