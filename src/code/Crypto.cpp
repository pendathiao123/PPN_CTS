#include <cstdlib>  // Pour rand() et srand()
#include <ctime>    // Pour time()
#include "../headers/Crypto.h"
#include "../headers/global.h"
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

// Méthode pour récupérer le solde actuel
double Crypto::getBalance(const std::string& currency) {
    if (currency == "DOLLARS") {
        return 1000.0;  // Simuler un solde de dollars
    }
    return 0.0;
}

// Méthode pour vendre une crypto
void Crypto::sellCrypto(const std::string& crypto, double percentage) {
    std::cout << "Vente de " << percentage << "% de " << crypto << std::endl;
    // Ajouter la logique de vente ici
}

// Méthode pour acheter une crypto
void Crypto::buyCrypto(const std::string& crypto, double percentage) {
    std::cout << "Achat de " << percentage << "% de " << crypto << std::endl;
    // Ajouter la logique d'achat ici
}
