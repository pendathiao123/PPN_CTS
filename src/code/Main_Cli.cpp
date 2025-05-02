#include <iostream>
#include "../headers/Client.h"
#include "../headers/Bot.h"

// le capital initial du client
#define CAPITAL_INIT 10000

int main() {
    Client client{7191};
    /**
     * Au debut le client n'a pas de Token/Mot de passe
     * Et on lui donne l'adresse et le port pour se connecter au serveur
     */
    client.StartClient("127.0.0.1", 4433);
    
    /**
     * Le Client avant de commencer a faire des échanges, doit alimenter son compte
     * avec de l'argent réel (soit en $). 
    */
    client.inject(10000); // on injecte 10 000$ dans notre compte

    // fonctions d'achat/vente clasiques
    client.buy("SRD-BTC", 5);
    //client.buy("SRD-BTC", 5);
    //client.sell("SRD-BTC", 51);
    //client.sell("SRD-BTC", 50);
    /**
     * Dans les focntions d'investissement et de trading,
     * on laisse au Bot le soin de calculer les montants idéaux
    */
    client.trade();
    //client.trade();
    //client.trade();
    //client.trade();
    //client.invest();
    

    // Finalement on recupére notre argent:
    double capital_final = client.withdraw();
    // Pour une bonne terminaison
    client.EndClient();

    // a-t-on été rentables ?
    std::cout << "Le client à fait un gain de " << (capital_final - CAPITAL_INIT) << std::endl;

    return 0;
}
