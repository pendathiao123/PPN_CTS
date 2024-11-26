#include <cstdlib>  // Pour rand() et srand()
#include <ctime>    // Pour time()
#include "../headers/Crypto.h"
#include "../headers/global.h"
#include "../headers/bot.h"
#include <fstream>
#include <atomic>
#include <chrono>
#include <thread>

// Constructeurs
Crypto::Crypto() : name(""), price(0.0), changeRate(0.0) {}

Crypto::Crypto(const std::string& name, double initialPrice, double changeRate)
    : name(name), price(initialPrice), changeRate(changeRate) {}

// Retourne le nom de la crypto
std::string Crypto::getName() const {
    return name;
}

// Retourne le prix actuel de la crypto

double Crypto::getPrice(const std::string& currency) const {
    if (currency == "SRD-BTC") {
        double price = 45.00 + static_cast<double>(rand());
        return price;
    } else {
        return false;
    }
}

// Met à jour le prix en fonction du taux de variation
void Crypto::updatePrice() {
    price += price * (changeRate / 100);  // Le prix augmente selon le taux de variation
}

// Affiche les informations sur la crypto
void Crypto::displayInfo() const {
    std::cout << "Crypto: " << name << ", Prix: " << price << ", Taux de variation: " << changeRate << "%" << std::endl;
}

// Méthode pour récupérer le dernier prix enregistré (par exemple, de fichier ou base de données)
double Crypto::get_prv_price(const std::string& currency) {
    // Logique pour obtenir le dernier prix enregistré
    std::ifstream inFile("SRD-BTC.dat");
    if (!inFile) {
        std::cerr << "Erreur lors de l'ouverture du fichier SRD-BTC.dat" << std::endl;
        return -1;
    }

    double price;
    std::string line;
    while (std::getline(inFile, line)) {
        // Lecture du fichier pour obtenir le dernier prix
        size_t pos = line.find_last_of(' ');
        if (pos != std::string::npos) {
            price = std::stod(line.substr(pos + 1));
        }
    }
    return price;  // Retourner le dernier prix enregistré
}

// Méthode pour vendre une crypto

bot1{"SRD-BTC"};

void Crypto::sellCrypto(const std::string& crypto, double percentage) {
    std::cout << "Vente de " << percentage << "% de " << crypto << std::endl;
    // Ajouter la logique de vente ici

    double solde_crypto = bot1.getBalance("SRD-BTC");
    double quantite = solde_crypto*percentage;
    std::unordered_map<std::string, double> bot_balance = bot1.get_total_Balance();
    
    bot_balance["SRD-BTC"] = bot_balance["SRD-BTC"] - quantite;
    
    double val2 = quantite*getPrice("SRD-BTC");

    bot_balance["DOLLARS"] = bot_balance["DOLLARS"] + val2;

    bot1.updateBalance(bot_balance); 
}

// Méthode pour acheter une crypto

bot bot1{"SRD-BTC"};

void Crypto::buyCrypto(const std::string& crypto, double percentage) {
    std::cout << "Achat de " << percentage << "% de " << crypto << std::endl;
    // Ajouter la logique d'achat ici

    double solde_dollars = bot1.getBalance("DOLLARS");
    double val1 = solde_dollars*percentage;
    std::unordered_map<std::string, double> bot_balance = bot1.get_total_Balance();
    
    bot_balance["DOLLARS"] = bot_balance["DOLLARS"] - val1;
    
    double val2 = getPrice("SRD-BTC");
    double quantite = val1/val2;

    bot_balance["SRD-BTC"] = bot_balance["SRD-BTC"] + quantite;

    bot1.updateBalance(bot_balance);   
}

// Fonction pour mettre à jour les prix de Bitcoin en continu et les enregistrer dans un fichier
void updateBitcoinPrices() {
    std::string filename = "SRD-BTC.dat";
    std::ofstream outFile(filename, std::ios::app);
    if (!outFile) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << ".\n";
        return;
    }
    Crypto crypto;
    int i = 0;
    while (!stopRequested) {
        double bitcoinPrice = crypto.getPrice("SRD-BTC");
        std::time_t timestamp = std::time(nullptr);
        outFile << timestamp << " " << bitcoinPrice << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Pause d'une seconde
        if (++i >= 10000) {  // Arrêter après 10000 itérations
            stopRequested = true;
            std::cout << "Fin de la mise à jour des prix de Bitcoin.\n";
        }
    }
    }
