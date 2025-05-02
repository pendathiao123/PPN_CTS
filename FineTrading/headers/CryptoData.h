#ifndef CRYPTO_DATA_H
#define CRYPTO_DATA_H

#include <vector>

// Déclaration des tableaux globaux (ou mieux, encapsulés dans une classe/struct si tu préfères)
// Note: Utiliser des variables globales n'est pas idéal, mais on garde ta structure pour l'instant.
extern std::vector<double> BTCValuesNormal;
extern std::vector<double> BTCValues3PercentNoise;
extern std::vector<double> BTCValues6PercentNoise;

// Déclaration de la fonction pour générer les valeurs
void generateBTCValues(int numValues = 2160); // 90 jours * 24 heures approx.

// On pourrait ajouter des fonctions pour récupérer un des vectors spécifiques si besoin
const std::vector<double>& getBTCValuesNormal();
const std::vector<double>& getBTCValues3PercentNoise();
const std::vector<double>& getBTCValues6PercentNoise();


#endif // CRYPTO_DATA_H