#ifndef BOT_H
#define BOT_H

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

class Client;

class Bot
{
private:
    // montant du solde original
    double solde_origin;
    // prix antérieur
    double prv_price;
    // solde actuel du Bot
    std::unordered_map<std::string, double> balances;
    // reference vers le Client qui détient le Bot
    std::shared_ptr<Client> client;
    std::mutex balanceMutex;

public:
    // nom du fichier contenant valeurs (passées) de la cryptomonaie
    static const std::string BTC_VALUES_FILE;

    // Constructeur par défaut
    Bot();
    // Constructeur avec un argument
    Bot(const std::string &currency);
    // Constructeur avec un pointeur vers Client
    Bot(Client* client);

    // Destructeur
    ~Bot();

    // Retourne le solde total du bot
    std::unordered_map<std::string, double> get_total_Balance();
    // Retourne le solde pour une devise spécifique
    double getBalance(const std::string &currency);
    // Met à jour le solde du bot
    void updateBalance(std::unordered_map<std::string, double> bot_balance);
    // Fonction de trading du bot
    void trading();
    // Fonction d'investissement du bot
    void investing();
    void investingLoop();
    // Retourne le prix de la devise spécifiée
    double getPrice(const std::string &currency);
    // Achète de la crypto-monnaie
    void buyCrypto(const std::string &currency, double pourcentage);
    // Vend de la crypto-monnaie
    void sellCrypto(const std::string &currency, double pourcentage);
};

#endif // BOT_H