#ifndef SRD_BTC_H
#define SRD_BTC_H

#include <map>


// Déclarations des fonctions

float getRandomFloat(float a);
float get_daily_BTC_value(int d);
void Complete_BTC_value();

// Fonction pour obtenir la valeur complète du BTC pour une seconde donnée le jour 0 à partir d'un fichier CSV
double get_complete_BTC_value(int d, int t);

// Fonction pour afficher la valeur du SRD-BTC
void SRD_BTC();

#endif // GLOBAL_H
