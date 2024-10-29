#ifndef MARKET_H
#define MARKET_H

#include <vector>
#include <string>
#include "Crypto.h"  // Inclut la définition de la classe Crypto

class Market {
private:
    std::vector<Crypto> cryptos;  // Vecteur contenant les objets Crypto

public:
    Market(const std::string& configFile);  // Constructeur pour charger les cryptomonnaies depuis un fichier
    const std::vector<Crypto>& getCryptos() const;  // Getter pour obtenir le vecteur de cryptomonnaies
};

#endif  // MARKET_H
