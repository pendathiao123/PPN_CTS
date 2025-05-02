#include <iostream>
#include "../headers/Client.h"
#include "../headers/Bot.h"


int main() {
    Client client{7201};
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
    client.buy("SRD-BTC", 50);
    //client.buy("SRD-BTC", 5);
    client.sell("SRD-BTC", 51);
    client.sell("SRD-BTC", 20);
    /**
     * Dans les focntions d'investissement et de trading, pour le moment on insère pas de valeur 
     * on laisse au Bot le soin de calculer les montants idéaux
    */
    
    //client.invest();
    //client.trade();
    // Pour une bonne terminaison
    client.EndClient();
    return 0;
}
