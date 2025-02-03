#ifndef BOT_H
#define BOT_H

#include <unordered_map>
#include <string>
#include <memory>

class Client;

class Bot
{
public:
    // nom du fichier contenant valeurs (passées) de la cryptomonaie
    static const std::string BTC_VALUES_FILE;

    Bot();
    Bot(const std::string &currency);
    ~Bot();

    std::unordered_map<std::string, double> get_total_Balance();
    double getBalance(const std::string &currency);
    void updateBalance(std::unordered_map<std::string, double> bot_balance);
    void trading();
    void investing();
    double getPrice(const std::string &currency);
    void buyCrypto(const std::string &currency, double pourcentage);
    void sellCrypto(const std::string &currency, double pourcentage);

private:
    double solde_origin;
    double prv_price;
    std::unordered_map<std::string, double> balances;
    std::shared_ptr<Client> client;
};

#endif // BOT_H