#include "../headers/Bot.h"
#include "../headers/Client.h"
#include "../headers/Global.h"
#include "../headers/SRD_BTC.h"
#include <iostream>
#include <unordered_map>
#include <thread>
#include <ctime>
#include <mutex>

// Initialisation des constantes
const std::string Bot::BTC_VALUES_FILE = "../src/data/btc_data.csv";
std::mutex balanceMutex; // Mutex pour protéger l'accès aux balances
// Constructeur par défaut de la classe Bot
Bot::Bot() : client(std::make_unique<Client>())
{
    solde_origin = 10000.0f;
    prv_price = 0.0f;
    balances = {
        {"SRD-BTC", 0.0},
        {"DOLLARS", 10000.0}};
}

// Constructeur de la classe Bot avec un paramètre de devise
Bot::Bot(const std::string &currency) : client(std::make_unique<Client>())
{
    solde_origin = 10000.0f;
    prv_price = 0.0f;
    balances = {
        {currency, 0.0},
        {"DOLLARS", 10000.0}};
}

// Constructeur de la classe Bot avec un pointeur vers un client existant
Bot::Bot(Client *client) : client(client)
{
    solde_origin = 10000.0f;
    prv_price = 0.0f;
    balances = {
        {"SRD-BTC", 0.0},
        {"DOLLARS", 10000.0}};
}

// Destructeur de la classe Bot
Bot::~Bot() {}

// Retourne la balance totale du bot
std::unordered_map<std::string, double> Bot::get_total_Balance()
{
    std::lock_guard<std::mutex> lock(balanceMutex); // Verrouiller l'accès aux balances
    return balances;
}

// Retourne la balance pour une devise spécifique
double Bot::getBalance(const std::string &currency)
{
    std::lock_guard<std::mutex> lock(balanceMutex); // Verrouiller l'accès aux balances
    if (balances.find(currency) != balances.end())
    {
        return balances[currency];
    }
    return 0.0;
}

// Met à jour la balance du bot
void Bot::updateBalance(std::unordered_map<std::string, double> bot_balance)
{
    std::lock_guard<std::mutex> lock(balanceMutex); // Verrouiller l'accès aux balances
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
        if (evolution >= 1.005)
        {
            std::cout << "Evolution >= 1.005, Selling 100%" << std::endl;
            sellCrypto("SRD-BTC", 100);
        }
        else if (evolution <= 0.995)
        {
            std::cout << "Evolution <= 0.995, Buying 5%" << std::endl;
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

// Boucle d'investissement du bot
void Bot::investingLoop()
{
    while (true)
    {
        investing();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Retourne le prix actuel de la devise
double Bot::getPrice(const std::string &currency)
{
    std::time_t currentTime = std::time(0);
    std::tm *now = std::localtime(&currentTime);
    int day = now->tm_yday;
    int second = now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec;

    if (currency == "SRD-BTC")
    {
        return get_complete_BTC_value(day, second); // Obtenir la valeur complète du BTC pour le jour et la seconde donnés
    }
    else
    {
        return 0.0;
    }
}

// Fonction d'achat de crypto-monnaie
void Bot::buyCrypto(const std::string &currency, double pourcentage)
{
    // Vérifier les limites d'achat globales et par client
    std::lock_guard<std::mutex> purchaseLock(Global::purchaseMutex);
    double amountToBuy = (pourcentage / 100.0) * getBalance("DOLLARS") / getPrice(currency);
    if (Global::globalDailyBTCPurchased + amountToBuy > Global::GLOBAL_DAILY_BTC_LIMIT)
    {
        std::cerr << "Limite globale d'achat de BTC atteinte pour la journée." << std::endl;
        return;
    }
    if (Global::clientDailyBTCPurchased[client->getId()] + amountToBuy > Global::CLIENT_DAILY_BTC_LIMIT)
    {
        std::cerr << "Limite d'achat de BTC atteinte pour le client " << client->getId() << " pour la journée." << std::endl;
        return;
    }

    std::string address;
    int port = 0; // Initialisation par défaut

    if (client->isConnected())
    {
        address = client->getServerAdress();
        port = client->getServerPort();
        std::cout << "Adresse récupérée: " << address << ", Port récupéré: " << port << std::endl;
    }
    else
    {
        std::cout << "Client non connecté au moment de la récupération de l'adresse et du port." << std::endl;
    }

    std::cout << "Passage dans Bot::buyCrypto " << std::endl;
    std::cout << "Adresse: " << address << ", Port: " << port << std::endl;

    if (address.empty() || port == 0)
    {
        std::cerr << "Adresse ou port invalide: Adresse: " << address << ", Port: " << port << std::endl;
        return; // Sortir de la fonction si l'adresse ou le port est invalide
    }

    if (!client->isConnected())
    {
        std::cout << "Tentative de connexion au client avec Adresse: " << address << ", Port: " << port << std::endl;
        client->StartClient(address, port, client->getId(), client->getToken());
    }

    client->buy(currency, pourcentage);

    // Mettre à jour les registres d'achat après l'achat
    std::lock_guard<std::mutex> updateLock(Global::purchaseMutex);
    Global::globalDailyBTCPurchased += amountToBuy;
    Global::clientDailyBTCPurchased[client->getId()] += amountToBuy;
}

// Fonction de vente de crypto-monnaie
void Bot::sellCrypto(const std::string &currency, double pourcentage)
{
    std::string address;
    int port = 0; // Initialisation par défaut

    if (client->isConnected())
    {
        address = client->getServerAdress();
        port = client->getServerPort();
        std::cout << "Adresse récupérée: " << address << ", Port récupéré: " << port << std::endl;
    }
    else
    {
        std::cout << "Client non connecté au moment de la récupération de l'adresse et du port." << std::endl;
    }

    std::cout << "Passage dans Bot::sellCrypto " << std::endl;
    std::cout << "Adresse: " << address << ", Port: " << port << std::endl;

    if (address.empty() || port == 0)
    {
        std::cerr << "Adresse ou port invalide: Adresse: " << address << ", Port: " << port << std::endl;
        return; // Sortir de la fonction si l'adresse ou le port est invalide
    }

    if (!client->isConnected())
    {
        std::cout << "Tentative de connexion au client avec Adresse: " << address << ", Port: " << port << std::endl;
        client->StartClient(address, port, client->getId(), client->getToken());
    }

    client->sell(currency, pourcentage);
}