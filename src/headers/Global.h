#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex> 
#include <thread> 
#include <memory> 

#include "../headers/Logger.h" 


class Global {
private:
    // Membres statiques pour les données partagées
    static std::mutex srdMutex;
    static double lastSRDBTCValue;

    static const int MAX_VALUES_PER_DAY = 5760; 
    static std::vector<double> ActiveSRDBTC; // Buffer circulaire
    static std::atomic<int> activeIndex;   // Index dans le buffer

    // Flag atomique pour signaler l'arrêt du thread de génération de prix
    static std::atomic<bool> stopRequested;

    // Membre statique pour le thread géré par Global elle-même
    static std::thread priceGenerationWorker;


    // Callback pour libcurl (déclaration)
    // Note : L'implémentation de cette fonction sera dans Global.cpp
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    // Fonction pour générer et mettre à jour le prix SRD-BTC (exécutée dans le thread interne)
    // Reste privée car elle est lancée par le thread interne de Global.
    static void generate_SRD_BTC_loop_impl();


public:
    // Méthodes statiques pour démarrer/arrêter le thread interne de Global
    // Appelées par le serveur au démarrage et à l'arrêt.
    static void startPriceGenerationThread();
    static void stopPriceGenerationThread();

    // Méthodes statiques pour accéder aux prix (utilisées par les Bots, etc.)
    // Elles sont thread-safe grâce au mutex et aux atomics.
    static double getPrice(const std::string& currency);
    static double getPreviousPrice(const std::string& currency, int secondsBack);

    // Méthode statique pour accéder au flag d'arrêt (principalement utilisée en interne par generate_SRD_BTC_loop_impl)
    static std::atomic<bool>& getStopRequested();

    // Désactiver le constructeur et le destructeur si Global est une classe utilitaire statique pure
    // Cela empêche la création d'instances de Global.
    Global() = delete;
    ~Global() = delete;
};

#endif