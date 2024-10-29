#include "Crypto.h"
#include <cstdlib>  // Pour rand() et srand()
#include <ctime>    // Pour time()

// Constructeur
Crypto::Crypto(const std::string& name, double initialPrice, double changeRate)
    : name(name), price(initialPrice), changeRate(changeRate) {}

// Retourne le nom de la crypto
std::string Crypto::getName() const {
    return name;
}

// Retourne le prix actuel
double Crypto::getPrice(const std::string& currency) const {
    if (currency == "SRD-BTC") {
        double price = 45.00 ;
        return price;
    } else {
        return false;
    }
}

// Met à jour le prix selon le taux de variation
void Crypto::updatePrice() {
    // Génère un changement aléatoire dans la plage [-changeRate, +changeRate]
    double randomChange = (static_cast<double>(rand()) / RAND_MAX) * 2 * changeRate - changeRate;
    price += price * (randomChange / 100.0);  // Mise à jour du prix
}

// Affiche les informations sur la crypto
void Crypto::displayInfo() const {
    std::cout << name << " : " << price << " USD" << std::endl;
}
