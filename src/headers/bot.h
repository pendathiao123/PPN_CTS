#ifndef BOT_HPP
#define BOT_HPP

#include <iostream>
#include <string>
#include "../headers/Crypto.h" 

class bot : public Crypto
{
public:
    bot(const std::string& currency);  // Utilisation de std::string pour la crypto
    ~bot();
    
    void trading();
    void investing();
    
    void sellCrypto(const std::string& crypto, double percentage);
    void buyCrypto(const std::string& crypto, double percentage);
    
private:
    float solde_origin;
    float prv_price;
};

#endif  // BOT_HPP
