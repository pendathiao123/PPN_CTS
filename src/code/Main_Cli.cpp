#include <iostream>
#include "../headers/Client.h"
#include "../headers/Bot.h"

// nombre de clients au total
#define NUM_CLI 1E1
// le capital initial de chaque client
#define CAPITAL_INIT 1E4
// nombre de demandes de trading pour chaque client
#define TTT 1E0


int main() {

    std::cout << "\n==============================================\n\n";

    for(int i=0; i<NUM_CLI; ++i){
        
        Client client{};
        /**
         * Au debut le client n'a pas de Token/Mot de passe
         * Et on lui donne l'adresse et le port pour se connecter au serveur
         */
        if(client.StartClient("127.0.0.1", 4433)){
            return 1;
        }

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
        for(int j=0; j<TTT; j++)
            client.trade();

        // Finalement on recupére notre argent:
        //capital_actu = client.withdraw(); // utile si on pense se reconecter une autre fois
        double capital_final = client.recoverAll(); // pour evaluer le gain total

        // Pour une bonne terminaison
        client.EndClient();

        // a-t-on été rentables ?
        std::cout << "Le client " << client.getId() << " à fait un gain de " << ((capital_final + capital_actu) - CAPITAL_INIT) << "\n";
    }
    std::cout << "\n==============================================" << std::endl;

    return 0;
}
