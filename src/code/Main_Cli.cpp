#include <iostream>
#include "../headers/Client.h"
#include "../headers/Bot.h"

// le capital initial du client
#define CAPITAL_INIT 10000

int main() {
    Client client{7182};
    /**
     * Au debut le client n'a pas de Token/Mot de passe
     * Et on lui donne l'adresse et le port pour se connecter au serveur
     */
    client.StartClient("127.0.0.1", 4433);
    double capital_actu = 0;
    
    /**
     * Le Client avant de commencer a faire des échanges, doit alimenter son compte
     * avec de l'argent réel (soit en $). 
    */
    client.inject(CAPITAL_INIT); // on injecte 10 000$ dans notre compte

    // fonctions d'achat/vente clasiques
    client.buy("SRD-BTC", 20);
    //client.sell("SRD-BTC", 20);

    /**
     * Dans la focntion de trading,
     * on laisse au Bot le soin de calculer les montants idéaux
    */
    for(int i=0;i<10;i++)
        client.trade();

    // Finalement on recupére notre argent:
    //capital_actu = client.withdraw(); // utile si on pense se reconecter une autre fois
    double capital_final = client.recoverAll(); // pour evaluer le gain total

    // Pour une bonne terminaison
    client.EndClient();

    // a-t-on été rentables ?
    std::cout << "Le client à fait un gain de " << ((capital_final + capital_actu) - CAPITAL_INIT) << std::endl;

    return 0;
}
