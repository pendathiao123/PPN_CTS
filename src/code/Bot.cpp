#include "../headers/Bot.h"
#include "../headers/Client.h"
#include "../headers/Global.h"
#include "../headers/SRD_BTC.h"
#include <iostream>
#include <unordered_map>
#include <ctime>

// Initialisation des constantes
const std::string Bot::BTC_VALUES_FILE = "../src/data/btc_data.csv";

// Constructeur par défaut
Bot::Bot() : client(std::make_shared<Client>())
{
    solde_origin = 1000.0f;
    prv_price = 0.0f;
    balances = {
        {"SRD-BTC", 0.0},
        {"DOLLARS", 1000.0}};
    Global::populateBTCValuesFromCSV("../src/data/btc_data.csv");
}

// Constructeur avec un argument
Bot::Bot(const std::string &currency) : client(std::make_shared<Client>())
{
    solde_origin = 1000.0f;
    prv_price = 0.0f;
    balances = {
        {currency, 0.0},
        {"DOLLARS", 1000.0}};
    Global::populateBTCValuesFromCSV("../src/data/btc_data.csv");
}

// Destructeur
Bot::~Bot()
{
}

// Retourne le solde total du bot
std::unordered_map<std::string, double> Bot::get_total_Balance()
{
    return balances;
}

// Retourne le solde pour une devise spécifique
double Bot::getBalance(const std::string &currency)
{
    if (balances.find(currency) != balances.end())
    {
        return balances[currency];
    }
    return 0.0;
}

// Met à jour le solde du bot
void Bot::updateBalance(std::unordered_map<std::string, double> bot_balance)
{
    balances = bot_balance;
}

// Fonction de trading du bot
void Bot::trading()
{
    for (int t = 300; t < 339292800; t += 300)
    {
        double solde = getBalance("DOLLARS");
        double price = getPrice("SRD-BTC");
        double evolution = 1 + ((price - prv_price) / price);

        if (solde > 0.5 * solde_origin)
        {
            if (evolution >= 1.02)
            {
                sellCrypto("SRD-BTC", 100);
            }
            else if (evolution <= 0.98)
            {
                buyCrypto("SRD-BTC", 5);
            }
        }
        else
        {
            if (evolution >= 1.04)
            {
                sellCrypto("SRD-BTC", 80);
            }
            else if (evolution <= 0.96)
            {
                buyCrypto("SRD-BTC", 3);
            }
        }
        prv_price = price;
    }
}

// Fonction d'investissement du bot
void Bot::investing()
{
    std::cout << "Passage dans Bot::investing " << std::endl;

    double solde = getBalance("DOLLARS");
    double price = getPrice("SRD-BTC");
    double evolution = 1 + ((price - prv_price) / price);

    std::cout << "Solde: " << solde << ", Price: " << price << ", Evolution: " << evolution << std::endl;

    if (solde > 0.5 * solde_origin)
    {
        std::cout << "Solde > 0.5 * solde_origin" << std::endl;
        if (evolution >= 1.02)
        {
            std::cout << "Evolution >= 1.02, Selling 100%" << std::endl;
            sellCrypto("SRD-BTC", 100);
        }
        else if (evolution <= 0.98)
        {
            std::cout << "Evolution <= 0.98, Buying 5%" << std::endl;
            buyCrypto("SRD-BTC", 5);
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
            sellCrypto("SRD-BTC", 80);
        }
        else if (evolution <= 0.96)
        {
            std::cout << "Evolution <= 0.96, Buying 3%" << std::endl;
            buyCrypto("SRD-BTC", 3);
        }
        else
        {
            std::cout << "No action taken" << std::endl;
        }
    }
    prv_price = price;
    std::cout << "Fin de l'itération de Bot::investing " << std::endl;
}

// Retourne le prix de la devise spécifiée
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

// Achète de la crypto-monnaie
void Bot::buyCrypto(const std::string &currency, double pourcentage)
{
    std::cout << "Passage dans Bot::buyCrypto " << std::endl;
    if (!client->isConnected())
    {
        client->StartClient("127.0.0.1", 4433, "7474", "77d7728205464e7791c58e510d613566874342c26413f970c45d7e2bc6dd9836");
    }
    client->buy(currency, pourcentage);
}

// Vend de la crypto-monnaie
void Bot::sellCrypto(const std::string &currency, double pourcentage)
{
    std::cout << "Passage dans Bot::sellCrypto " << std::endl;
    if (!client->isConnected())
    {
        client->StartClient("127.0.0.1", 4433, "7474", "77d7728205464e7791c58e510d613566874342c26413f970c45d7e2bc6dd9836");
    }
    client->sell(currency, pourcentage);
}