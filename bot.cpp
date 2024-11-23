#include<iostream>
#include<string>
#include"Crypto.h"
#include"bot.hpp"
#include"Portfolio.h"


float solde_origin;
float prv_price;


void trading()                               // Le trading bot vérifie plus fréquemment l'état du Market, et investit de plus petites sommes.
{
    for (int t=300; t<339292800; t=t+300)
    {
        auto solde = getBalance("DOLLARS");                   // fonction qui retourne la valeur de son solde actuel. Cette valeur sera réactualisée après chaque buy ou sell.
        auto price = getPrice("SRD-BTC");                     // valeur de la monnaie au temps t où la fonction est appelée
        auto prv_price = get_prv_price("SRD-BTC");            // check_market est une fonction qui renvoie la valeur du SRD-BTC au temps t (peut-être avec un pointeur ?)
        auto evolution = 1 + ((price - prv_price)/price);
        if (solde > 0.5*solde_origin)
        {   
            if (evolution >= 1.02)
            {
                sellCrypto("SRD-BTC",100);          //Met en lien le portefolio crypto et le porte monnaie du client. Elle prend en entrée la crypto, le % qu'il souhaite vendre, fait une conversion en dollar et ajoute ce résultat dans le porte monnaie réel.
            }
            else if (evolution <= 0.98)
            {    
                buyCrypto("SRD-BTC",5);             // Même principe, mais avec un transfert inverse.
            }
        }
        else
        {
            if (evolution >= 1.04)
            {
                sellCrypto("SRD-BTC",80);      
            }
            else if (evolution <= 0.96)
            {    
                buyCrypto(3);             
            }
        }
    }
}


void investing()                                              // Le bot investment vérifie moins fréquemment l'état du Market(une par jour), et investit de plus grosses sommes.
{
    for (int t=86400; t<339292800; t=t+86400)
    {
        auto solde = getBalance("DOLLARS");                   // fonction qui retourne la valeur de son solde actuel. Cette valeur sera réactualisée après chaque buy ou sell.
        auto price = getPrice("SRD-BTC");                     // valeur de la monnaie au temps t où la fonction est appelée
        auto prv_price = get_prv_price("SRD-BTC");            // check_market est une fonction qui renvoie la valeur du SRD-BTC au temps t (peut-être avec un pointeur ?)
        auto evolution = 1 + ((price - prv_price)/price);
        if (solde > 0.5*solde_origin)
        {   
            if (evolution >= 1.02)
            {
                sellCrypto("SRD-BTC",100);          //Met en lien le portefolio crypto et le porte monnaie du client. Elle prend en entrée la crypto, le % qu'il souhaite vendre, fait une conversion en dollar et ajoute ce résultat dans le porte monnaie réel.
            }
            else if (evolution <= 0.98)
            {    
                buyCrypto("SRD-BTC",5);             // Même principe, mais avec un transfert inverse.
            }
        }
        else
        {
            if (evolution >= 1.04)
            {
                sellCrypto("SRD-BTC",80);      
            }
            else if (evolution <= 0.96)
            {    
                buyCrypto(3);             
            }
        }
    }
}