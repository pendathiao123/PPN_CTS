#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>
#include <iostream>
#include <array>
#include <unordered_map>

#define CRYPTO_MAX 100

class Crypto {
private:
    std::string name;     // Nom de la crypto
    double price;         // Prix actuel de la crypto
    double changeRate;    // Taux de variation du prix (en pourcentage)

    // variable qui simule l'evolution des valeurs des crypto-monaies
    long int current_value;

public:
    // Constructeurs
    Crypto();
    Crypto(const std::string& name, double initialPrice, double changeRate);

    // Getter pour le nom de la crypto
    std::string getName() const;

    // retourne la valeur de la crypto pour current_value
    double get_SRD_BTC_value();

    // Getteur pour le prix actuel
    double getPrice(const std::string& currency);

    // Mise à jour du prix selon le taux de variation
    void updatePrice();

    // Affichage des informations sur la crypto
    void displayInfo() const;

    // Méthode qui fait evouler le prix des cryptos à chaque appel
    void retroActivitySim();
    
};

#endif  // CRYPTO_H