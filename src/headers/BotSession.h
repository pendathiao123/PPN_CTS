#ifndef BOTSESSION_H
#define BOTSESSION_H

#include <string>
#include <memory> 
#include <thread> 
#include <atomic> 

// Déclarations anticipées pour les classes utilisées par référence ou pointeur
class Client;
class Bot;
class TransactionQueue;

#include "../headers/Client.h" 
#include "../headers/Bot.h"    
#include "../headers/TransactionQueue.h" 
#include "../headers/Logger.h" 


class BotSession {
private:
    std::string clientId; // ID du client associé à cette session
    std::shared_ptr<Client> client; // Objet Client gérant la communication (shared ownership)
    std::shared_ptr<Bot> bot;       // Objet Bot gérant la logique de trading (shared ownership)

    std::thread sessionThread;    // Thread pour exécuter la logique de la session (ex: trading du bot)
    std::atomic<bool> running;    // Flag atomique pour signaler si la session est active et doit continuer son thread

    // Méthode exécutée par le thread sessionThread (la logique de trading du bot)
    void run();

public:
    // Constructeur
    BotSession(std::shared_ptr<Client> client_ptr, const std::string& id);

    // Destructeur : s'assure que le thread s'arrête et sauvegarde le bot
    ~BotSession();

    // Démarre le thread de la session
    void start();

    // Demande l'arrêt du thread de la session
    void stop();

    // Indique si la session est active et son thread doit continuer
    bool isRunning() const;

    // Retourne l'ID du client
    const std::string& getId() const; // Utilise getId(), pas getClientId() pour correspondre au header/cpp

    // Permet d'accéder à l'objet Bot associé
    std::shared_ptr<Bot> getBot() const;

    // Permet d'accéder à l'interface de communication Client
    std::shared_ptr<Client> getClient() const; // <<<--- RENOMMÉ de getClientCommunicationInterface


    // Méthode appelée par la TransactionQueue pour appliquer une requête de transaction
    // Cette méthode s'exécute dans le thread(s) de la TransactionQueue !
    void applyTransactionRequest(const TransactionRequest& req);
};

#endif