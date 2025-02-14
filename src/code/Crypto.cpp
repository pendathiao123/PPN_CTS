#include <cstdlib> // Pour rand() et srand()
#include <ctime>   // Pour time()
#include <fstream>
#include <atomic>
#include <chrono>
#include <thread>
#include "../headers/Crypto.h"
#include "../headers/Bot.h"

// Constructeurs
Crypto::Crypto() : name(""), price(0.0), changeRate(0.0), balances(balances) {}
Crypto::Crypto(const std::string &name, double initialPrice, double changeRate)
    : name(name), price(initialPrice), changeRate(changeRate), balances(balances) {}

// Retourne le nom de la crypto
std::string Crypto::getName() const
{
    return name;
}

// Retourne le prix actuel de la crypto
double Crypto::getPrice(const std::string &currency) const
{
    if (currency == "SRD-BTC")
    {
        double price = 45.00 + static_cast<double>(rand());
        return price;
    }
    else
    {
        return false;
    }
}

// Met à jour le prix en fonction du taux de variation
void Crypto::updatePrice()
{
    price += price * (changeRate / 100); // Le prix augmente selon le taux de variation
}

// Affiche les informations sur la crypto
void Crypto::displayInfo() const
{
    std::cout << "Crypto: " << name << ", Prix: " << price << ", Taux de variation: " << changeRate << "%" << std::endl;
}

// Méthode pour récupérer le dernier prix enregistré (par exemple, de fichier ou base de données)
double Crypto::get_prv_price(const std::string &currency)
{
    // Logique pour obtenir le dernier prix enregistré
    std::ifstream inFile("SRD-BTC.dat");
    if (!inFile)
    {
        std::cerr << "Erreur lors de l'ouverture du fichier SRD-BTC.dat" << std::endl;
        return -1;
    }

    double price;
    std::string line;
    while (std::getline(inFile, line))
    {
        // Lecture du fichier pour obtenir le dernier prix
        size_t pos = line.find_last_of(' ');
        if (pos != std::string::npos)
        {
            price = std::stod(line.substr(pos + 1));
        }
    }
    return price; // Retourner le dernier prix enregistré
}

Bot bot1{"SRD-BTC"};

// Méthode pour vendre une crypto
void Crypto::sellCrypto(const std::string &crypto, double percentage)
{
    std::cout << "Vente de " << percentage << " pourcent de " << crypto << std::endl;
    // Ajouter la logique de vente ici

    double solde_crypto = bot1.getBalance("SRD-BTC");
    double quantite = solde_crypto * percentage;
    std::unordered_map<std::string, double> bot_balance = bot1.get_total_Balance();

    bot_balance["SRD-BTC"] = bot_balance["SRD-BTC"] - quantite;
    Crypto cryptosell;
    double val2 = quantite * cryptosell.getPrice("SRD-BTC");

    bot_balance["DOLLARS"] = bot_balance["DOLLARS"] + val2;

    bot1.updateBalance(bot_balance);
}

// Méthode pour acheter une crypto
void Crypto::buyCrypto(const std::string &crypto, double percentage)
{
    std::cout << "Achat de " << percentage << " pourcent de " << crypto << std::endl;
    // Ajouter la logique d'achat ici

    double solde_dollars = bot1.getBalance("DOLLARS");
    double val1 = solde_dollars * percentage;
    std::unordered_map<std::string, double> bot_balance = bot1.get_total_Balance();

    bot_balance["DOLLARS"] = bot_balance["DOLLARS"] - val1;
    Crypto cryptobuy;
    double val2 = cryptobuy.getPrice("SRD-BTC");
    double quantite = val1 / val2;

    bot_balance["SRD-BTC"] = bot_balance["SRD-BTC"] + quantite;

    bot1.updateBalance(bot_balance);
}