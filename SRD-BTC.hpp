#include<iostream>
#include<map>


float getRandomFloat(float, float);

float BTC_value();                          //Cette fonction simule la valeur du BTC à chaque seconde

float get_BTC_value(int);                   //Cette fonction retourne la valeur du Bitcoin pour le jour d correspondant. Elle ira la chercher avec un pointeur.

void SRD_BTC();                             //La fonction affiche la valeur du SRD-BTC en utilisant les deux fonctions précédentes.


std::map<int, double> BTC_daily_values;

std::map<std::pair<int, int>, double> BTC_sec_values;