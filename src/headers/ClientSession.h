#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <string>
#include <memory> 
#include <atomic> 
#include <thread> 
#include <mutex> 

#include "Global.h"     
#include "Server.h"     
#include "Wallet.h"     
#include "Bot.h"       
#include "TransactionQueue.h" 
#include "Transaction.h" 


// ClientSession gère la session d'un client connecté, son authentification,
// son Wallet, et potentiellement un bot de trading.
// Hérite de enable_shared_from_this pour obtenir un shared_ptr vers soi-même
// et le passer à des objets (comme Bot) qui ont une durée de vie potentiellement plus courte.
class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    // Constructeur
    ClientSession(const std::string& clientId, std::shared_ptr<ServerConnection> clientConn, std::shared_ptr<Wallet> wallet);

    // Destructeur
    ~ClientSession();

    // Méthode pour lancer le thread principal de la session (appelée par Server)
    void start();
    // Méthode pour signaler l'arrêt du thread et attendre sa fin (appelée par Server ou destructeur)
    void stop();

    // La boucle principale d'exécution de la session (dans son propre thread)
    void run(); // Exécute sessionLoop

    // Traite une commande reçue du client (ex: SHOW WALLET, BUY ...)
    void processClientCommand(const std::string& command);

    // Gère la requête de trading manuelle du client. Calcule la quantité réelle et soumet à la TQ.
    // 'value' est soit un pourcentage soit une quantité.
    bool handleClientTradeRequest(RequestType type, const std::string& cryptoName, double value);

    // Appelée par la TransactionQueue lorsqu'une transaction est appliquée pour ce client.
    // Notifie le bot si présent, et envoie le résultat au client.
    void applyTransactionRequest(const Transaction& tx);

    // --- Méthodes spécifiques au bot ---
    // Appelée suite à la commande client "START BOT <K>". Crée et démarre l'objet Bot.
    void startBot(double bollingerK);

    // Appelée suite à la commande client "STOP BOT". Signale l'arrêt au bot et le détruit.
    void stopBot();

    // Appelée par le Bot pour soumettre un ordre décidé par sa logique.
    // Ne fait PAS de vérification finale de fonds ici. Soumet la requête à la TQ.
    void submitBotOrder(TradingAction action);


    // --- Getters ---
    const std::string& getClientId() const;
    std::shared_ptr<Wallet> getClientWallet() const;
    std::shared_ptr<ServerConnection> getClientConnection() const; // Retourne le shared_ptr du ServerConnection


private:
    // La méthode principale exécutée par le thread de la session (contient la boucle de réception réseau)
    void sessionLoop();

    // --- Membres de la session ---
    std::string clientId; // ID du client associé à cette session
    std::shared_ptr<ServerConnection> client; // Connexion réseau (TCP+SSL)
    std::shared_ptr<Wallet> clientWallet;     // Portefeuille du client associé
    std::shared_ptr<Bot> bot;                 // Objet Bot géré par cette session (nullptr si pas de bot actif)

    std::thread sessionThread; // Thread d'exécution de cette session
    std::atomic<bool> running; // Flag atomique pour signaler l'arrêt du thread de la session

    // Le mutex pour la map de sessions est géré dans Server/TransactionQueue, pas ici.
};

#endif