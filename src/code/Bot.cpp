#include "../headers/Bot.h"
#include "../headers/Client.h"
#include "../headers/Global.h"
#include <iostream>
#include <unordered_map>
#include <ctime>

// Initialisation des constantes
const std::string Bot::BTC_VALUES_FILE = "../src/data/btc_data.csv";

Bot::Bot() : prv_price(0.) {}

// Destructeur
Bot::~Bot()
{
}


// Fonction de trading du bot
void Bot::trading(Crypto &cry, int &action, double &q, const double dollars, const double srd_btc)
{
    /** Dans cette fonction nous allons itérer pour observer la tendance du SRD-BTC
     * Si on observe une tendance, on peut effectuer une action.
     * Sinon on va iterer sans rien faire.
     * Cet algo ne fonctionne que si on a des dollars et des cryptos.
    */
    // verification initiale
    if((dollars == 0) || (srd_btc == 0)){ // si l'une des deux valeurs est nulle
        return; // on sort
    }
    // initialisation des varaibles
    prv_price = cry.getPrice("SRD-BTC");
    cry.retroActivitySim(); // évolution de la valeur de la crypto
    double price = cry.getPrice("SRD-BTC");
    signed int indice_conf = 0; // indice de confiance
    double comp;

    for(int t = 0; t < MAX_ITER; ++t)
    {
        // evolution prix de la crypto
        comp = price - prv_price;

        // actualisation de l'indice de confiance
        if(comp > 0){ // si la valeur augmente
            ++indice_conf;
        }else if(comp < 0){
            --indice_conf;
        }

        if(indice_conf <= -3){ // tendence de la crypto: valeur décroissante
            // on vend une partie de ce qu'on a avant que ça ne perde trop de valeur
            action = 2; // code de vente
            q = srd_btc / 3; // quantité vendue
            return; 
        }else if(indice_conf >= 5){ // tendence de la crypto: valeur croissante
            // on achette un peu de crypto avant que ça devienne trop cher
            double achat_max = dollars / price;
            double degre = 1;
            if(achat_max > degre){ // pour ne pas aller sur des subdivisions trop petites
                while(achat_max > degre){ // avoir une echélle de combien on peut en acheter
                    degre *= 10;
                }
                action = 1; // code d'achat
                q = degre * 0.1 * 0.2; // quantité acheté
                return;
            }
            // sinon on achete pas
            std::cout << "No action taken" << std::endl;
            return;
        }

        cry.retroActivitySim(); // évolution de la valeur de la crypto
        prv_price = price;
        price = cry.getPrice("SRD-BTC");
    }
    std::cout << "No action taken" << std::endl;
}
