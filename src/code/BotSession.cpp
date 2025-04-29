#include "../headers/BotSession.h"
#include "../headers/Bot.h"          
#include "../headers/Client.h"        
#include "../headers/Transaction.h"   
#include "../headers/Global.h"        
#include "../headers/TransactionQueue.h" 
#include "../headers/Logger.h"        

#include <iostream>
#include <chrono>
#include <utility> 
#include <thread> 


// Déclaration de la file de transactions globale
extern TransactionQueue txQueue;


// Constructeur de BotSession
BotSession::BotSession(std::shared_ptr<Client> client_ptr, const std::string& id)
    : clientId(id), 
      client(client_ptr), 
      bot(std::make_shared<Bot>(id)), 
      running(false) 
{
    LOG("BotSession créée pour client ID: " + clientId, "DEBUG");
    // Passer la file de transactions globale au Bot
    if (bot) {
        bot->setTransactionQueue(&txQueue);
        LOG("TransactionQueue définie pour le bot ID: " + clientId, "DEBUG");
    } else {
         LOG("Erreur critique: Bot n'a pas pu être créé pour client ID: " + clientId, "ERROR");
    }
    // Le thread sessionThread est créé et démarré par la méthode start().
}

// Destructeur de BotSession
BotSession::~BotSession() {
    LOG("Destructeur BotSession appelé pour client ID: " + clientId, "DEBUG");
    // stop() est appelée pour s'assurer que le thread s'arrête
    stop();
    // Attendre que le thread sessionThread se termine si il est joignable
    if (sessionThread.joinable()) {
        LOG("Jointure du thread BotSession pour client ID: " + clientId + "...", "DEBUG");
        sessionThread.join();
        LOG("Thread BotSession joint pour client ID: " + clientId + ".", "DEBUG");
    }

    // Sauvegarder le solde du bot quand la session se termine
    if (bot) { // Vérifier que le bot existe
        bot->updateBalance(); // Appelle saveBalance() qui utilise déjà balanceMutex
         LOG("Solde du bot ID: " + clientId + " sauvegardé à la fin de session.", "INFO");
    } else {
         LOG("Erreur : Bot est null dans le destructeur de BotSession pour client ID: " + clientId, "ERROR");
    }

    // Désenregistrement de la TransactionQueue est géré par le Server dans HandleClient/HandleAuthenticatedClient
    // Retrait de la map activeSessions du Server est géré par le Server.

    /*Les shared_ptr 'client' et 'bot' seront automatiquement détruits ici, libérant les ressources 
    qu'ils gèrent si ce shared_ptr était le dernier. Le destructeur de Client gérera la fermeture du socket et la libération SSL.*/
}

// Démarre le thread de la session
void BotSession::start() {
    if (!running.load()) { // Vérifier l'état actuel pour éviter de démarrer plusieurs fois
        running = true;
        // Créer et démarrer le thread, en lui passant la fonction run()
        sessionThread = std::thread(&BotSession::run, this); // Passer 'this' pour que run soit une méthode membre
        LOG("Thread BotSession démarré pour client ID: " + clientId, "INFO");
    } else {
         LOG("Appel à start() sur une BotSession déjà démarrée (client ID: " + clientId + "). Ignoré.", "WARNING");
    }
}

// Demande l'arrêt du thread de la session
void BotSession::stop() {
     // Utiliser running.load() pour vérifier l'état atomique avant de changer
    if (running.load()) {
        running = false; // Indiquer au thread de s'arrêter
        LOG("Demande d'arrêt de BotSession pour client ID: " + clientId, "INFO");
        // Si le thread est bloqué dans un appel de sleep ou autre, il se réveillera et vérifiera 'running'
        // Si bloqué dans des opérations d'I/O non interruptibles, cela pourrait prendre plus de temps
    } else {
         LOG("Appel à stop() sur une BotSession déjà arrêtée (client ID: " + clientId + "). Ignoré.", "WARNING");
    }
}

// Fonction exécutée par le thread sessionThread (logique de trading du bot)
void BotSession::run() {
    LOG("Thread BotSession::run démarré pour client ID: " + clientId, "DEBUG");

    // Le thread continue tant que le flag running est true
    // La détection de la déconnexion client est gérée par HandleAuthenticatedClient qui appellera session->stop() si receive échoue
    while (running.load()) {

        // Vérifier si le bot est valide avant de faire du trading
        if (bot) {
             // Exécuter la logique de trading du bot
             // La méthode trading soumettra des requêtes à la TransactionQueue
             bot->trading("SRD-BTC"); // Utilise la crypto hardcodée pour l'instant
        } else {
             LOG("Erreur : Bot est null dans BotSession::run pour client ID: " + clientId, "ERROR");
             // Demander l'arrêt de la session en cas d'erreur critique
             stop(); // Arrête ce thread
             break; // Sortir de la boucle
        }

        // Pause pour contrôler la fréquence de trading du bot
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Si on sort de la boucle (running false), le thread s'arrête.
    LOG("Thread BotSession::run terminé pour client ID: " + clientId, "INFO");
    // La sauvegarde du solde et le nettoyage sont gérés dans le destructeur de BotSession.
}

// Indique si le thread de la session est en cours d'exécution
bool BotSession::isRunning() const {
    // La session est considérée comme running si son flag l'indique ET que le serveur global tourne.
    // On suppose qu'un flag global server_running existe (par exemple dans main_Serv.cpp)
    extern std::atomic<bool> server_running; // Déclaration si ce flag est global

    return running.load() && server_running.load(); // Dépend du FLAG GLOBAL server_running
}

// Retourne l'ID du client (déjà nommé getId dans ton header/cpp)
const std::string& BotSession::getId() const { 
    return clientId;
}

// Permet d'accéder à l'objet Bot
std::shared_ptr<Bot> BotSession::getBot() const {
    return bot;
}

// Permet d'accéder à l'interface de communication Client
// Renommée getClient() pour correspondre à l'utilisation dans HandleAuthenticatedClient.
std::shared_ptr<Client> BotSession::getClient() const { // <<<--- RENOMMÉ de getClientCommunicationInterface
    return client;
}


// Méthode appelée par la TransactionQueue pour appliquer une requête de transaction
// Cette méthode s'exécute dans le thread(s) de la TransactionQueue
void BotSession::applyTransactionRequest(const TransactionRequest& req) {
    if (!bot) {
        LOG("[BotSession::applyTransactionRequest] Erreur : Bot est null pour client ID: " + clientId + ". Requête ignorée.", "ERROR");
        return;
    }

    LOG(std::string("[BotSession::applyTransactionRequest] Application transaction (") + (req.type == RequestType::BUY ? "BUY" : "SELL") + ") pour client ID: " + clientId, "INFO");

    /* Accéder au bot et modifier son solde en utilisant les méthodes get/setBalance 
    qui maintenant utilisent un mutex interne au Bot. C'est thread-safe.*/

    if (req.type == RequestType::BUY) {
        double prix = Global::getPrice(req.cryptoName);
        double pourcentage = req.quantity; // Pourcentage de capital

        // Utilise getBalance et setBalance qui sont thread-safe (grâce au mutex interne du Bot)
        double currentDollars = bot->getBalance("DOLLARS");
        double amountInDollars = currentDollars * pourcentage;

        // Vérification finale du solde juste avant la modification
        if (amountInDollars > 0 && amountInDollars <= currentDollars) {
            double cryptoQuantityBought = amountInDollars / prix;
            bot->setBalance("DOLLARS", currentDollars - amountInDollars); // Met à jour le solde DOLLARS
            bot->setBalance(req.cryptoName, bot->getBalance(req.cryptoName) + cryptoQuantityBought); // Met à jour le solde crypto

            // Créer et logguer l'objet Transaction. logTransactionToCSV doit être thread-safe.
            Transaction tx(clientId, "BUY", req.cryptoName, cryptoQuantityBought, prix); // Ajout totalAmount
            tx.logTransactionToCSV("../src/data/transactions.csv"); // Chemin hardcodé

            LOG("[BotSession::applyTransactionRequest] Achat appliqué. Client ID: " + clientId + ", Montant DOLLARS: " + std::to_string(amountInDollars) + ", Quantité Crypto: " + std::to_string(cryptoQuantityBought), "INFO");

        } else {
            LOG("[BotSession::applyTransactionRequest] Achat refusé : Solde DOLLARS insuffisant ou montant invalide pour client ID: " + clientId + ". Solde actuel: " + std::to_string(currentDollars), "WARNING");
        }

    } else if (req.type == RequestType::SELL) {
        double prix = Global::getPrice(req.cryptoName);
        double pourcentage = req.quantity; // Pourcentage de crypto

        // Utilise getBalance et setBalance qui sont thread-safe
        double currentCrypto = bot->getBalance(req.cryptoName);
        double amountInCrypto = currentCrypto * pourcentage;

        // Vérification finale du solde
        if (amountInCrypto > 0 && amountInCrypto <= currentCrypto) {
            double dollarsReceived = amountInCrypto * prix;
            bot->setBalance(req.cryptoName, currentCrypto - amountInCrypto); // Met à jour solde crypto
            bot->setBalance("DOLLARS", bot->getBalance("DOLLARS") + dollarsReceived); // Met à jour solde DOLLARS

            // Créer et logguer l'objet Transaction. logTransactionToCSV doit être thread-safe.
            Transaction tx(clientId, "SELL", req.cryptoName, amountInCrypto, prix); // Ajout totalAmount
            tx.logTransactionToCSV("../src/data/transactions.csv"); // Chemin hardcodé

             LOG("[BotSession::applyTransactionRequest] Vente appliquée. Client ID: " + clientId + ", Quantité Crypto: " + std::to_string(amountInCrypto) + ", Montant DOLLARS: " + std::to_string(dollarsReceived), "INFO");

        } else {
             LOG("[BotSession::applyTransactionRequest] Vente refusée : Solde Crypto insuffisant ou montant invalide pour client ID: " + clientId + ". Solde actuel: " + std::to_string(currentCrypto) + " en " + req.cryptoName, "WARNING");
        }
    }
    // La sauvegarde du solde sur disque est gérée à la déconnexion/arrêt de session.
}