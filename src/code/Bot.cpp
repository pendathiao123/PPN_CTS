#include "../headers/Bot.h"
#include "../headers/Client.h"
#include "../headers/Global.h"
#include "../headers/SRD_BTC.h"
#include <iostream>
#include <unordered_map>
#include <ctime>

// Initialisation des constantes
const std::string Bot::BTC_VALUES_FILE = "../src/data/btc_data.csv";

Bot::Bot(){
    // quand on demarre un Bot, toutes ses valeurs sont à zéro.
    solde_origin = 0.0f;
    prv_price = 0.0f;
    balances = {
        {"SRD-BTC", 0.0},
        {"DOLLARS", 0.0}};
    Global::populateBTCValuesFromCSV(BTC_VALUES_FILE);
}

// Destructeur
Bot::~Bot()
{
}



std::unordered_map<std::string, double> Bot::get_total_Balance()
{
    return balances;
}


double Bot::getBalance(const std::string &currency)
{
    if (balances.find(currency) != balances.end())
    {
        return balances[currency];
    }
    return 0.0;
}


void Bot::updateBalance(std::unordered_map<std::string, double> bot_balance)
{
    balances = bot_balance;
}

// Fonction de trading du bot
void Bot::trading(Crypto &cry, int &action, double &q, const double dollars, const double srd_btc)
{
    /** Dans cette fonction nous allons itérer pour observer la tendance du SRD-BTC
     * Si on observe une tendance, on peut effectuer une action.
     * Sinon on va iterer sans rien faire.
     * Cet algo ne fonctionne que si on a des dollars et des cryptos.
    */
    // verification initiale
    if((dollars == 0) || (srd_btc == 0)){ // si l'une des deux valeurs est nulle
        return; // on sort
    }
    // initialisation des varaibles
    prv_price = cry.getPrice("SRD-BTC");
    cry.retroActivitySim(); // évolution de la valeur de la crypto
    double price = cry.getPrice("SRD-BTC");
    signed int indice_conf = 0; // indice de confiance
    double comp;

    for(int t = 0; t < MAX_ITER; ++t)
    {
        // evolution prix de la crypto
        comp = price - prv_price;

        // actualisation de l'indice de confiance
        if(comp > 0){ // si la valeur augmente
            ++indice_conf;
        }else if(comp < 0){
            --indice_conf;
        }

        if(indice_conf <= -3){ // tendence de la crypto: valeur décroissante
            // on vend une partie de ce qu'on a avant que ça ne perde trop de valeur
            action = 2; // code de vente
            q = srd_btc / 3; // quantité vendue
            return; 
        }else if(indice_conf >= 5){ // tendence de la crypto: valeur croissante
            // on achette un peu de crypto avant que ça devienne trop cher
            double achat_max = dollars / price;
            double degre = 1;
            if(achat_max > degre){ // pour ne pas aller sur des subdivisions trop petites
                while(achat_max > degre){ // avoir une echélle de combien on peut en acheter
                    degre *= 10;
                }
                action = 1; // code d'achat
                q = degre * 0.1 * 0.2; // quantité acheté
                return;
            }
            // sinon on achete pas
            return;
        }

        cry.retroActivitySim(); // évolution de la valeur de la crypto
        prv_price = price;
        price = cry.getPrice("SRD-BTC");
    }
}


void Bot::investing(Crypto &cry, int &action, double &q, const double dollars, const double srd_btc)
{
    std::cout << "Passage dans Bot::investing " << std::endl;

    double price = cry.getPrice("SRD-BTC");
    double evolution = 1 + ((price - prv_price) / price);

    std::cout << "Solde: " << dollars << ", Price: " << price << ", Evolution: " << evolution << std::endl;

    if (dollars > 0.5 * 100) // à retravailler
    {
        std::cout << "Solde > 0.5 * solde_origin" << std::endl;
        if (evolution >= 1.005)
        {
            std::cout << "Evolution >= 1.005, Selling 100%" << std::endl;
            // Vente de "SRD-BTC"
            action = 2; // code de vente
            q = 100; // quantité vendue
        }
        else if (evolution <= 0.995)
        {
            std::cout << "Evolution <= 0.995, Buying 5%" << std::endl;
            // Achat de "SRD-BTC"
            action = 1; // code d'achat
            q = 5; // quantité acheté
        }
        else
        {
            std::cout << "No action taken" << std::endl;
        }
    }
    else
    {
        std::cout << "Solde <= 0.5 * solde_origin" << std::endl;
        if (evolution >= 1.04)
        {
            std::cout << "Evolution >= 1.04, Selling 80%" << std::endl;
            // Vente de "SRD-BTC"
            action = 2; // code de vente
            q = 100; // quantité vendue
        }
        else if (evolution <= 0.96)
        {
            std::cout << "Evolution <= 0.96, Buying 3%" << std::endl;
            // Achat de "SRD-BTC"
            action = 1; // code d'achat
            q = 5; // quantité acheté
        }
        else
        {
            std::cout << "No action taken" << std::endl;
        }
    }
    prv_price = price;
    std::cout << "Fin de l'itération de Bot::investing " << std::endl;
}

/*
double Bot::getPrice(const std::string &currency)
{
    std::time_t currentTime = std::time(0);
    std::tm *now = std::localtime(&currentTime);
    int day = now->tm_yday;
    int second = now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec;

    if (currency == "SRD-BTC")
    {
        // Global::readBTCValuesFromCSV("btc_sec_values.csv");
        return get_complete_BTC_value(day, second);
    }
    else
    {
        return 0.0;
    }
}
*/