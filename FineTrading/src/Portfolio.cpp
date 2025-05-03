#include "Portfolio.h"
#include <iostream>
#include <string>
#include <fstream>
#include <iomanip> // Pour std::setprecision

Portfolio::Portfolio(double initial_balance)
    : balance(initial_balance), btcAmount(0.0) {}

void Portfolio::updateBalance(double amount) {
    balance += amount;
}

void Portfolio::updateBTC(double amount) {
    btcAmount += amount;
}

double Portfolio::getBalance() const {
    return balance;
}

double Portfolio::getBTC() const {
    return btcAmount;
}

// Nouvelle fonction pour obtenir la valeur totale du portefeuille
double Portfolio::getTotalValue(double currentMarketPrice) const {
    return balance + (btcAmount * currentMarketPrice);
}


void Portfolio::displayPortfolio() const {
    std::cout << std::fixed << std::setprecision(2); // Affichage propre
    std::cout << "  Solde: " << balance << " USDT\n";
    std::cout << "  BTC: " << std::setprecision(8) << btcAmount << " BTC\n"; // Plus de précision pour BTC
    // Optionnel : Afficher la valeur totale
    // double currentValue = getTotalValue(lastKnownPrice); // Nécessiterait de connaître le dernier prix ici
    // std::cout << "  Valeur Totale (approx): " << currentValue << " USDT\n";
}

void Portfolio::buyBTC(double marketPrice, double percentOfBalance) {
    if (marketPrice <= 0) {
         std::cerr << "[Erreur Achat] Prix du marché invalide.\n";
         return;
    }
     if (percentOfBalance <= 0.0 || percentOfBalance > 1.0) {
        // Permettre 1.0 mais pas plus
         std::cerr << "[Erreur Achat] Pourcentage d'achat invalide (doit être > 0 et <= 1.0).\n";
         return;
    }

    double amountToSpend = balance * percentOfBalance;
    if (amountToSpend <= 0.0) { // Ne rien faire si on n'a rien à dépenser
         // std::cout << "[Info Achat] Montant à dépenser nul ou négatif.\n";
         return;
     }
     if (amountToSpend > balance) {
         // Sécurité, même si percentOfBalance <= 1.0 devrait le garantir
         amountToSpend = balance;
     }

    double btcBought = amountToSpend / marketPrice;

    balance -= amountToSpend;
    btcAmount += btcBought;

    std::cout << std::fixed << std::setprecision(8);
    std::cout << "[Achat] " << btcBought << " BTC acheté pour " << std::setprecision(2) << amountToSpend << " USDT @ " << marketPrice << " USDT/BTC.\n";
    // displayPortfolio(); // Optionnel: afficher le portefeuille après chaque transaction
}

void Portfolio::sellBTC(double marketPrice, double percentOfBTC) {
     if (marketPrice <= 0) {
         std::cerr << "[Erreur Vente] Prix du marché invalide.\n";
         return;
    }
    if (percentOfBTC <= 0.0 || percentOfBTC > 1.0) {
         std::cerr << "[Erreur Vente] Pourcentage de vente invalide (doit être > 0 et <= 1.0).\n";
        return;
    }

    if (btcAmount <= 0.0) { // Ne rien faire si on n'a pas de BTC à vendre
         // std::cout << "[Info Vente] Aucun BTC à vendre.\n";
        return;
    }

    double btcToSell = btcAmount * percentOfBTC;
     if (btcToSell > btcAmount) {
         // Sécurité
         btcToSell = btcAmount;
     }
     if (btcToSell <= 0.0) {
         return; // Ne rien faire si la quantité à vendre est nulle (ex: btcAmount était très petit)
     }

    double usdtReceived = btcToSell * marketPrice;

    btcAmount -= btcToSell;
    balance += usdtReceived;

    std::cout << std::fixed << std::setprecision(8);
    std::cout << "[Vente] " << btcToSell << " BTC vendu pour " << std::setprecision(2) << usdtReceived << " USDT @ " << marketPrice << " USDT/BTC.\n";
    // displayPortfolio(); // Optionnel
}


// *** Fonction CSV maintenant en dehors de la classe ***
void saveResultsToCSV(int runNumber, const std::string& strategyType, const std::string& dataType, double btcVar, double portfolioVar) {
    // Utilise ton chemin de fichier
    std::string filePath = "/home/ark30/Files/PPN/FineTrading/results.csv";
    // Ouvre en mode ajout (append)
    std::ofstream csv(filePath, std::ios::app);

    if (!csv.is_open()) {
        std::cerr << "Erreur critique: Impossible d'ouvrir le fichier CSV: " << filePath << std::endl;
        return;
    }

    // Vérifier si le fichier est vide pour écrire l'en-tête
    csv.seekp(0, std::ios::end); // Va à la fin
    if (csv.tellp() == 0) { // Si la position est 0, le fichier est vide
        csv << "Run,Strategy,DataType,BTCChange%,PortfolioChange%\n";
    }

    // Écrit la nouvelle ligne de données
    csv << runNumber << ","
        << strategyType << ","
        << dataType << ","
        << std::fixed << std::setprecision(4) << btcVar << "," // Précision pour les pourcentages
        << std::fixed << std::setprecision(4) << portfolioVar << "\n";

    csv.close(); // Bonne pratique de fermer le fichier après écriture
}