#include <iostream>
#include "../headers/bot.h"
#include "../headers/Crypto.h"
#include "../headers/Client.h"


bot::bot(){}

// Constructeur de la classe bot
bot::bot(const std::string& currency) {
    solde_origin = 1000.0f;  // Solde initial
    prv_price = 0.0f;        // Prix précédent initialisé à 0
    balances = {
    {"SRD-BTC", 0.0}, 
    {"DOLLARS", 1000.0}
    };
}

// Destructeur de la classe bot
bot::~bot() {
    // Libération des ressources si nécessaire
}

std::unordered_map<std::string, double> bot::get_total_Balance(){
        return balances;
    }


    // Méthode pour récupérer le solde actuel
    double bot::getBalance(const std::string& currency) {
        if (currency == "DOLLARS") {
            return balances["DOLLARS"];  
        }
        else if (currency == "SRD-BTC"){
            return balances["SRD-BTC"];     
        }
        else {return 0.0;}
    }


    void bot::updateBalance(std::unordered_map<std::string, double> bot_balance){
        balances["DOLLARS"] = bot_balance["DOLLARS"];
        balances["SRD-BTC"] = bot_balance["SRD-BTC"];
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

/*void bot::buyCrypto(const std::string& currency, double pourcentage) {
    std::cout << "Passage dans Bot::buyCrypto " << std::endl;
    // Construire la requête d'achat
    std::string request = "BUY " + currency + " " + std::to_string(pourcentage);
    
    // Envoyer la requête d'achat au serveur
    client.sendRequest(request);
    
    // Recevoir et afficher la réponse
    std::string response = client.receiveResponse();
}
void bot::sellCrypto(const std::string& currency, double pourcentage) {
    std::cout << "Passage dans Bot::sellCrypto " << std::endl;
    // Construire la requête d'achat
    std::string request = "SELL " + currency + " " + std::to_string(pourcentage);
    
    // Envoyer la requête d'achat au serveur
    client.sendRequest(request);
    
    // Recevoir et afficher la réponse
    std::string response = client.receiveResponse();
}*/
