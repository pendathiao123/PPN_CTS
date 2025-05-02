#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

// Includes standards nécessaires
#include <string>
#include <memory>          // shared_ptr, enable_shared_from_this
#include <thread>          // std::thread
#include <atomic>          // std::atomic<bool>
#include <vector>          // Pour std::vector si nécessaire dans les signatures (non visible ici)


// Includes des classes/composants liés - C'est le hub de liens!
#include "../headers/ServerConnection.h"         // Gère la connexion réseau brute
#include "../headers/Bot.h"            // Le bot associé à la session
#include "../headers/TransactionQueue.h" // Pour ajouter des requêtes et les structures associées
#include "../headers/Logger.h"         // Pour le logging (LOG macro)
#include "../headers/Wallet.h"         // Le portefeuille du client associé


class Server;
// --- Classe ClientSession : Gère toute l'interaction pour un client unique ---
/**
 * Gère la session individuelle, la communication, les commandes, bot et portefeuille.
 * Opère typiquement dans son propre thread.
 */
class ClientSession : public std::enable_shared_from_this<ClientSession> {
private:
    // --- Membres d'identification et de gestion de connexion/composants ---
    std::string clientId;                // L'ID du client
    std::shared_ptr<ServerConnection> client;      // L'objet gérant la connexion réseau brute

    std::shared_ptr<Wallet> clientWallet;  // Le portefeuille du client (chargé/sauvegardé par la session)
    std::shared_ptr<Bot> bot;              // Le bot associé (peut être nullptr si inactif)

    // --- Membres de gestion du thread de session ---
    std::thread sessionThread;           // Le thread dédié qui exécute la boucle principale
    std::atomic<bool> running;           // Flag atomique pour contrôler l'exécution du thread (thread-safe par nature)

    // --- Méthodes privées (logique interne du thread/traitement) ---
    void run();                        // La boucle principale du thread de session (réception commandes, gestion bot actif)
    void processClientCommand(const std::string& request); // Parse et dispatche les commandes du client
    bool handleClientTradeRequest(RequestType req_type, Currency trade_currency, double percentage); // Gère les commandes de trading (calcul et soumission TQ)
    bool submitBotOrder(TradingAction action);

public:
    // --- Constructeur et Destructeur ---
    // Le constructeur initialise la session, charge/crée le wallet. Le destructeur sauvegarde le wallet et gère le thread.
    ClientSession(std::shared_ptr<ServerConnection> client_ptr, const std::string& id, [[maybe_unused]] std::shared_ptr<Server> server, const std::string& dataDirPath); // Constructeur
    ~ClientSession(); // Destructeur

    // --- Contrôle du thread de session ---
    void start();     // Démarre le thread run()
    void stop();      // Demande l'arrêt propre et attend la fin du thread
    bool isRunning() const; // Vérifie si le thread est actif

    // --- Getters (accès aux composants associés) ---
    const std::string& getId() const; // Retourne l'ID client
    std::shared_ptr<Bot> getBot() const; // Retourne le pointeur Bot (peut être nullptr)
    std::shared_ptr<ServerConnection> getClientConnection() const; // Connection du client vu par le serveur
    std::shared_ptr<Wallet> getClientWallet() const; // Retourne le pointeur Wallet

    // --- Méthode de Callback (appelée par la TransactionQueue) ---
    // Le mécanisme par lequel la TQ notifie cette session du résultat d'une requête.
    void applyTransactionRequest(const Transaction& tx); // Applique le résultat d'une transaction (objet Transaction final) notifié par la TQ.

    // --- Gestion du Bot associé ---
    void startBot(int bollingerPeriod, double bollingerK); // Crée et démarre le bot
    void stopBot(); // Arrête et détruit le bot

    // Héritage de std::enable_shared_from_this<ClientSession> permet d'obtenir un shared_ptr<ClientSession> vers 'this',
    // ce qui est utile pour s'enregistrer auprès de la TransactionQueue (qui garde des weak_ptr).
};

#endif // CLIENTSESSION_H