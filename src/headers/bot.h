#ifndef BOT_HPP
#define BOT_HPP

#include <iostream>
#include <string>
#include <unordered_map>
#include "../headers/Crypto.h" 

class bot : public Crypto
{
public:
    bot(const std::string& currency);  // Utilisation de std::string pour la crypto
    ~bot();
    
    void trading();
    void investing();

    std::unordered_map<std::string, double> get_total_Balance();


    // Méthode pour récupérer le solde actuel
    double getBalance(const std::string& currency);


    void updateBalance(std::unordered_map<std::string, double> bot_balance);
    
private:
    double solde_origin;    // Solde initial par exemple
    double prv_price;       // Prix précédent initialisé à 0
    double solde_crypto;    //Quantité de crypto initiale
    std::unordered_map<std::string, double> balances;
};

#endif  // BOT_HPP
