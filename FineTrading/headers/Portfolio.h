#ifndef PORTFOLIO_H
#define PORTFOLIO_H

#include <string> // Ajout pour std::string dans saveResultsToCSV

class Portfolio {
private:
    double balance; // Solde en USDT (ou autre devise de base)
    double btcAmount; // Quantité de BTC détenue

public:
    // Constructeur
    Portfolio(double initial_balance);

    // Modificateurs (on pourrait les rendre privés et ne les utiliser qu'internement si besoin)
    void updateBalance(double amount);
    void updateBTC(double amount);

    // Accesseurs (Getters)
    double getBalance() const;
    double getBTC() const;
    double getTotalValue(double currentMarketPrice) const; // Nouvelle fonction utile

    // Actions de trading
    void buyBTC(double marketPrice, double percentOfBalance);
    void sellBTC(double marketPrice, double percentOfBTC);

    // Affichage
    void displayPortfolio() const;

    // ATTENTION: Cette fonction sera déplacée ou rendue statique
    // static void saveResultsToCSV(int runNumber, const std::string& strategyType, const std::string& dataType, double btcVar, double portfolioVar);
};

// Fonction utilitaire pour le CSV (déplacée hors de la classe)
void saveResultsToCSV(int runNumber, const std::string& strategyType, const std::string& dataType, double btcVar, double portfolioVar);


#endif // PORTFOLIO_H