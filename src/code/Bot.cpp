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


void Bot::trading(int &action, double &q, const double dollars, const double srd_btc)
{
    double price = cry.getPrice("SRD-BTC");
    double evolution = 1 + ((price - prv_price) / price);
    //for (int t = 300; t < 339292800; t += 300) // very long ~ 1 million
    for (int t = 300; t < 33929; t += 300)
    {
        price = cry.getPrice("SRD-BTC");
        evolution = 1 + ((price - prv_price) / price);

        if (dollars > 0.5 * 100.0) // ceci devra être retravaillé
        {
            if (evolution >= 1.02)
            {
                // Vente de "SRD-BTC"
                action = 2; // code de vente
                q = 100; // quantité vendue
            }
            else if (evolution <= 0.98)
            {
                // Achat de "SRD-BTC"
                action = 1; // code d'achat
                q = 5; // quantité acheté
            }
        }
        else
        {
            if (evolution >= 1.04)
            {
                // Vente de "SRD-BTC"
                action = 2; // code de vente
                q = 100; // quantité vendue
            }
            else if (evolution <= 0.96)
            {
                // Achat de "SRD-BTC"
                action = 1; // code d'achat
                q = 5; // quantité acheté
            }
        }
        prv_price = price;
    }
}


void Bot::investing(int &action, double &q, const double dollars, const double srd_btc)
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