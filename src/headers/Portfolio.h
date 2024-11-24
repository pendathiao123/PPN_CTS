#include<map>

class Portfolio {
private:
    std::map<std::string, double> holdings;  // Quantité détenue par crypto, dictionnaire qui stocke nom de la cypto, valeur

public:
    void buyCrypto(const std::string& name, double amount);
    void sellCrypto(const std::string& name, double amount);
    double getBalance(const std::string& name) const; //Retourne la quantité d'une cryptomonnaie détenue dans le portefeuille
};
