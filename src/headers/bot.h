#ifndef BOT_H
#define BOT_H

#include <iostream>
#include <string>
#include <unordered_map>
#include "../headers/Crypto.h" 
#include "../headers/Client.h"

class bot : public Crypto
{
public:
    bot();
    bot(const std::string& currency);  // Utilisation de std::string pour la crypto
    ~bot();
    Client client{};
    


    void trading();
    void investing();

    std::unordered_map<std::string, double> get_total_Balance();

    // Méthode pour récupérer le solde actuel
    double getBalance(const std::string& currency);
    
    void buyCrypto(const std::string& currency, double amount);
    void sellCrypto(const std::string& currency, double amount);
    void updateBalance(std::unordered_map<std::string, double> bot_balance);
    
private:
    double solde_origin;    // Solde initial par exemple
    double prv_price;       // Prix précédent initialisé à 0
    double solde_crypto;    //Quantité de crypto initiale
    std::unordered_map<std::string, double> balances;
    
};

#endif  // BOT_H
