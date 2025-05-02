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

    // prix antérieur
    double prv_price;

    // nombre maximal d'itérations pour le Bot
    const long int MAX_ITER = 1E03; // soit 1000

public:
    // nom du fichier contenant valeurs (passées) de la cryptomonaie
    static const std::string BTC_VALUES_FILE;

    // Constructeur par défaut
    Bot();

    // Destructeur
    ~Bot();

    // Fonction de trading du bot
    void trading(Crypto &cry, int &action, double &q, const double dollars, const double srd_btc);

};

#endif // BOT_H