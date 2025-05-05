#include <iostream>
#include <chrono>
#include <unistd.h>
#include "../headers/Client.h"


// nombre de clients au total
#define NUM_CLI 1E2
// le capital initial de chaque client
#define CAPITAL_INIT 1E3
// nombre de demandes de trading pour chaque client
#define TTT 1E0
// nombre de socondes de mesure pour les tests de performances
#define SEC 20

int main() {

    std::cout << "\n==============================================\n\n";
    std::cout << "========Bénefice pour chaque client (avec trading)=========\n";
    long int tps = 0;

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
        client.inject(CAPITAL_INIT); // on injecte 1000$ dans notre compte

        // achat initial
        client.buy("SRD-BTC", 10);

        /*======================Mesures de Performances=================================*/
        // on établi un duée de 60s
        /*
        long int tps_cli = 0;
        const std::chrono::duration<double> elapsed_time{std::chrono::seconds(SEC)};
        // depart
        const auto start{std::chrono::steady_clock::now()};
        auto end{std::chrono::steady_clock::now()};
        std::chrono::duration<double> compare_time{end - start};
        while(compare_time < elapsed_time){ // une durée de 10secondes minimum
            // fonctions d'achat/vente clasiques
            client.buy("SRD-BTC", 1);
            client.sell("SRD-BTC", 1);
            end = std::chrono::steady_clock::now();
            tps += 2;
            tps_cli += 2;
            compare_time = end - start;
        }
        /*
        long int cps = 0;
        const std::chrono::duration<double> elapsed_time{std::chrono::seconds(SEC)};
        // depart
        const auto start{std::chrono::steady_clock::now()};
        auto end{std::chrono::steady_clock::now()};
        std::chrono::duration<double> compare_time{end - start};
        while(compare_time < elapsed_time){
            // deconexion
            client.EndClient();
            client.StartClient("127.0.0.1", 4433);
            end = std::chrono::steady_clock::now();
            ++cps;
            compare_time = end - start;
        }
        */
        const std::chrono::duration<double> elapsed_time{std::chrono::seconds(i)};
        // depart
        const auto start{std::chrono::steady_clock::now()};
        auto end{std::chrono::steady_clock::now()};
        std::chrono::duration<double> compare_time{end - start};
        while(compare_time < elapsed_time){
            // fonctions de trading
            client.trade();
            end = std::chrono::steady_clock::now();
            compare_time = end - start;
        }

        /*===============================================================================*/

        /**
         * Dans la focntion de trading,
         * on laisse au Bot le soin de calculer les montants idéaux
        
        for(int j=0; j<TTT; j++){
            client.trade();
        }
        */
        

        // Finalement on recupére notre argent:
        //capital_actu = client.withdraw(); // utile si on pense se reconecter une autre fois
        double capital_final = client.recoverAll(); // pour evaluer le gain total

        // Pour une bonne terminaison
        client.EndClient();

        // a-t-on été rentables ?
        //std::cout << "Le client " << client.getId() << " à fait un gain de " << ((capital_final + capital_actu) - CAPITAL_INIT) << "\n";
        //std::cout << "Pour 10 secondes nous avons " << tps << " transactions.\n";
        //std::cout << "Client " << client.getId() << " TPS = " << (tps_cli / 60) << "\n";
        //std::cout << "Client " << client.getId() << " CPS = " << (cps / secs) << "\n";

        std::cout << client.getId() << " " << ((capital_final + capital_actu) - CAPITAL_INIT) << "\n";
        //std::cout << tps << " " << (tps_cli / 60) << "\n";
        //sleep(1);

    }
    std::cout << "\n==============================================" << std::endl;

    return 0;
}
