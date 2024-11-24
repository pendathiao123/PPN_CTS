#include<iostream>
#include<random>
#include <cstdlib>
#include <ctime>
#include "../headers/SRD_BTC.h"
#include "../headers/global.h"


float getRandomFloat(float a) 
{
    std::srand(static_cast<unsigned int>(std::time(0)));
    float randomFloat = static_cast<float>(std::rand()) / RAND_MAX * a;
    return randomFloat;
}

float get_daily_BTC_value(int d)     //Cette fonction retourne la valeur du Bitcoin pour le jour d correspondant. Elle ira la chercher avec un pointeur.
{ 
    double BTC_value = BTC_daily_values[d];
    return BTC_value;
}



void Complete_BTC_value()              //Cette fonction simule les valeurs du BTC à chaque seconde  
{
    for (int d=0; d<3927; ++d)          //Le nombre de jours totaux de la simulation complète
    {
        int t=0;
        float BTC_value = get_daily_BTC_value(d);
        for (t=0; t<86400; ++t)
        {
            if (t<14400)
            {BTC_value = BTC_value + ((getRandomFloat(t))/13000)*BTC_value;}        //La valeur 13000 est arbitraire, elle sert à atteindre les 5% de variation maximale sur cette periode précise de la journée
            else if (t>72000)
            {BTC_value = 0.95*BTC_value + ((getRandomFloat(86400-t))/13000)*BTC_value;}
            else
            {BTC_value = 0.9*BTC_value + (getRandomFloat(0.2))*BTC_value;}          //En milieu de journée, l'incertiude peut monter jusqu'à 10%
            BTC_sec_values[{d,t}]=BTC_value;
        }
    }
}

double get_complete_BTC_value(int d, int t)       //Cette fonction retourne la valeur du Bitcoin pour le jour d et la seconde s entrée en arg. Elle ira la chercher avec un pointeur.
{
    double BTC_sec_value = BTC_sec_values[{d,t}];
    return BTC_sec_value;
}



void SRD_BTC()    //La fonction affiche la valeur du SRD-BTC en utilisant les fonctions précédentes.
{
    float randomFloat = getRandomFloat(0.1022);
    double BTC_value = 0;
    double SRD_BTC_value = 0;
    for (int d=0; d<3927; ++d)                      //Le nombre de jours totaux de la simulation complète
    {
        int t=0;
        for (t=0; t<86400; ++t)
        {
            SRD_BTC_value = (0.9489 + randomFloat)*BTC_sec_values[{d,t}];
            std::cout<<SRD_BTC_value<<"\n";
        }
    }
}