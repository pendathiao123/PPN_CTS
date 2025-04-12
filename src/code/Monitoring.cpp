#include "../headers/Monitoring.h"
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>

#include "../headers/Bot.h" //pour atteindre getBalance
#include "../headers/Global.h" //pour lire fichier csv

using namespace prometheus;
Bot bot; //Nous permet d'accéder aux différentes fonctions (getPrice et get_total_Balance)

/*Fonction qui lance la récupération des données utiles qui seront visibles à l'adresse 127.0.0.1:8080*/
void PrometheusServer() {
    Exposer exposer{"127.0.0.1:8080"};
    auto registry = std::make_shared<Registry>();   //création d'un registre ou on met les gauges etc.. 

    //Balances clients
    auto& balance_gauge = BuildGauge()
        .Name("Balance_client")
        .Help("Balances d'un client")
        .Register(*registry);

    //SRD-BTC graphique
    auto& SRDBTC_gauge = BuildGauge()
        .Name("SRD_BTC")
        .Help("Graphique du SRD-BTC au cours du temps")
        .Register(*registry);
    auto& SRD_BTC_gauge = SRDBTC_gauge.Add({}); //permet de définir une gauge sans Labels (pas FamilyGauge)

    exposer.RegisterCollectable(registry);

    while (true) {
        std::unordered_map<std::string, double> balances = bot.get_total_Balance();

        /*mise à jour du solde des portefeuilles clients du point de vue de Prometheus; récupère les données*/
        for (const auto& [currency, balance] : balances) {
            balance_gauge
                .Add({{"currency", currency}})
                .Set(balance);
            std::cout << "Mise à jour du solde pour la devise " << currency << ": " << balance << std::endl;
        }

        /*Obtenir le prix du SRC-BTC*/
        double prix_btc = bot.getPrice("SRD-BTC");
        SRD_BTC_gauge.Set(prix_btc);
        std::cout << "Mise à jour du prix du SRD-BTC : " << prix_btc << std::endl; 

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}