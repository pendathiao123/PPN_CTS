// src/headers/ClientAuthenticator.h - En-tête de la classe ClientAuthenticator

#ifndef CLIENT_AUTHENTICATOR_H
#define CLIENT_AUTHENTICATOR_H

// --- Includes nécessaires pour les déclarations ---
#include <string>
#include <unordered_map> // Reste ici car AuthOutcomeToString utilise une map (potentiellement, ou si une autre méthode l'utilise)
#include <mutex>         // Reste ici si une autre méthode l'utilise
#include <memory>
#include <vector>

// Inclure les headers des autres composants avec lesquels ClientAuthenticator interagit DIRECTEMENT dans ses signatures :
#include "../headers/ServerConnection.h" // AuthenticateClient prend une référence `ServerConnection&`


// Forward declaration
class Server;


// --- Énumération AuthOutcome : Résultat d'une tentative d'authentification ---
enum class AuthOutcome {
    FAIL,    // Échec de l'authentification (ID ou mot de passe incorrect)
    SUCCESS, // Authentification réussie pour un utilisateur existant
    NEW,     // Authentification réussie et nouvel utilisateur créé
    UNKNOWN  // État inconnu ou erreur interne non gérée
};

// --- Déclaration de la fonction utilitaire pour convertir l'enum en string ---
std::string authOutcomeToString(AuthOutcome outcome);


// --- Classe ClientAuthenticator : Gère le protocole d'authentification ---
/**
 * @brief Gère la réception et le parsing du message d'authentification client.
 *
 * Cette classe se concentre sur l'interaction réseau initiale pour obtenir les
 * identifiants du client. La vérification et l'enregistrement des utilisateurs
 * sont délégués au Server.
 */
class ClientAuthenticator {
public:
    /**
     * @brief Constructeur par défaut.
     */
    ClientAuthenticator();

    /**
     * @brief Reçoit le message d'authentification, le parse, et demande au Server de vérifier/enregistrer l'utilisateur.
     *
     * Interagit avec le client via client_conn. Appelle les méthodes du Server pour la logique utilisateur.
     * En cas de succès, stocke l'ID et le token de session dans l'objet ServerConnection.
     * Gère l'envoi des messages d'échec au client et la fermeture de la connexion en cas d'échec d'auth.
     *
     * @param client_conn Référence à l'objet ServerConnection représentant la connexion du client.
     * Utilisé pour la communication (receive/send) et pour stocker l'ID/Token authentifiés si succès.
     * @param server_instance Référence à l'instance du Server pour appeler ses méthodes de gestion utilisateur.
     * @return Le résultat de l'authentification (AuthOutcome::SUCCESS, AuthOutcome::NEW, AuthOutcome::FAIL).
     */
    AuthOutcome AuthenticateClient(ServerConnection& client_conn, Server& server_instance, std::string& authenticatedUserId);


    // Note: Les méthodes internes de parsing, ou de sécurité des mots de passe (si elles restaient ici)
    // seraient privées. Dans cette refonte, la logique de sécurité mdp est déplacée dans Server.


}; // Fin de la classe ClientAuthenticator

#endif // CLIENT_AUTHENTICATOR_H