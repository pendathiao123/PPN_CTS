#include "../headers/bot.h"
#include <iostream>

// Constructeur de la classe bot
bot::bot(const std::string& currency) {
    solde_origin = 1000.0f;  // Solde initial par exemple
    prv_price = 0.0f;        // Prix précédent initialisé à 0
}

// Destructeur de la classe bot
bot::~bot() {
    // Libération des ressources si nécessaire
}

// Méthode de trading
void bot::trading() {
    for (int t = 300; t < 339292800; t += 300) {
        auto solde = getBalance("DOLLARS");
        auto price = getPrice("SRD-BTC");
        auto evolution = 1 + ((price - prv_price) / price);

        if (solde > 0.5 * solde_origin) {
            if (evolution >= 1.02) {
                sellCrypto("SRD-BTC", 100);  // Exemple de vente
            } else if (evolution <= 0.98) {
                buyCrypto("SRD-BTC", 5);     // Exemple d'achat
            }
        } else {
            if (evolution >= 1.04) {
                sellCrypto("SRD-BTC", 80);  // Exemple de vente
            } else if (evolution <= 0.96) {
                buyCrypto("SRD-BTC", 3);    // Exemple d'achat
            }
        }
    }
}

// Méthode d'investissement
void bot::investing() {
    for (int t = 86400; t < 339292800; t += 86400) {
        auto solde = getBalance("DOLLARS");
        auto price = getPrice("SRD-BTC");
        auto evolution = 1 + ((price - prv_price) / price);

        if (solde > 0.5 * solde_origin) {
            if (evolution >= 1.02) {
                sellCrypto("SRD-BTC", 100);
            } else if (evolution <= 0.98) {
                buyCrypto("SRD-BTC", 5);
            }
        } else {
            if (evolution >= 1.04) {
                sellCrypto("SRD-BTC", 80);
            } else if (evolution <= 0.96) {
                buyCrypto("SRD-BTC", 3);
            }
        }
    }
}

// Définition de la méthode sellCrypto
void bot::sellCrypto(const std::string& crypto, double percentage) {
    std::cout << "Vente de " << percentage << "% de " << crypto << std::endl;
    //logique a faire
}

// Définition de la méthode buyCrypto
void bot::buyCrypto(const std::string& crypto, double percentage) {
    std::cout << "Achat de " << percentage << "% de " << crypto << std::endl;
    //logique a faire
}
