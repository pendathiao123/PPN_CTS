#ifndef BOT_H
#define BOT_H

#include <unordered_map>
#include <string>
#include <memory>
#include "Crypto.h"


class Server; // ligne nécessaire pour referencer le Serveur !

class Bot
{
private:

    // reference du client pour qui le Bot fait les transactions
    //int client_Id;

    // montant du solde original
    double solde_origin;

    // prix antérieur
    double prv_price;

    // solde actuel du Bot
    std::unordered_map<std::string, double> balances;

    // nombre maximal d'itérations pour le Bot
    const long int MAX_ITER = 1E03; // soit 1000

public:
    // nom du fichier contenant valeurs (passées) de la cryptomonaie
    static const std::string BTC_VALUES_FILE;

    // Constructeur par défaut
    Bot();

    // Destructeur
    ~Bot();

    // Retourne le solde total du bot
    std::unordered_map<std::string, double> get_total_Balance();
    // Retourne le solde pour une devise spécifique
    double getBalance(const std::string &currency);
    // Met à jour le solde du bot
    void updateBalance(std::unordered_map<std::string, double> bot_balance);

    // Fonction de trading du bot
    void trading(Crypto &cry, int &action, double &q, const double dollars, const double srd_btc);

    // Retourne le prix de la devise spécifiée
    //double getPrice(const std::string &currency);
};

#endif // BOT_H