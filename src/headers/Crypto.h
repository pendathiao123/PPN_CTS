#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>
#include <iostream>

class Crypto {
private:
    std::string name;     // Nom de la crypto
    double price;         // Prix actuel de la crypto
    double changeRate;    // Taux de variation du prix (en pourcentage)
    std::unordered_map<std::string, double>& balances;    //Dictionnaire du portefolio

public:
    // Constructeurs
    Crypto();
    Crypto(const std::string& name, double initialPrice, double changeRate);

    // Getter pour le nom de la crypto
    std::string getName() const;

    // Getter pour le prix actuel
    double getPrice(const std::string& currency) const;

    // Mise à jour du prix selon le taux de variation
    void updatePrice();

    // Affichage des informations sur la crypto
    void displayInfo() const;

    // Méthode pour récupérer le dernier prix enregistré (pour le bot)
    static double get_prv_price(const std::string& currency);

    // Méthode pour vendre une crypto
    static void sellCrypto(const std::string& crypto, double percentage);

    // Méthode pour acheter une crypto
    static void buyCrypto(const std::string& crypto, double percentage);
};

#endif  // CRYPTO_H
