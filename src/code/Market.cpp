#include "Market.h"
#include "Crypto.h"  // Assurez-vous que ce fichier existe et contient la définition de la classe Crypto
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

// Constructeur qui charge les cryptomonnaies à partir d'un fichier de configuration
Market::Market(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << configFile << std::endl;
        return;
    }

    // Logique pour lire le fichier et initialiser le vecteur cryptos
    // Exemple : lecture ligne par ligne et création d'objets Crypto
    std::string name;
    double price;
    double changeRate;

    while (file >> name >> price >> changeRate) {
        cryptos.emplace_back(name, price, changeRate);  // Supposons que Crypto a un constructeur qui prend un nom et un prix,et un taux de variation
    }
}

// Retourne le vecteur de cryptomonnaies
const std::vector<Crypto>& Market::getCryptos() const {
    return cryptos;
}
