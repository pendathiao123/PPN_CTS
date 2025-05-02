#ifndef GLOBAL_H
#define GLOBAL_H

// Includes standards nécessaires pour les types et fonctionnalités
#include <string>
#include <vector>
#include <atomic> // Pour std::atomic (gestion thread-safe de bool/int simples)
#include <thread> // Pour std::thread (gestion du thread de fond)
#include <mutex> // Pour std::mutex (protection des données partagées complexes)
#include <cstddef> // Pour size_t

// Includes des classes/composants liés (raison de l'inclusion pas toujours évidente dans ce .h seul)
#include "Transaction.h" // Contient l'enum Currency et la classe Transaction

// --- Classe Global : Fournisseur de données et services globaux (gestion des prix, threads) ---
// Cette classe utilise principalement des membres et méthodes statiques.
class Global {
private:
    // --- Membres statiques pour la gestion du thread de génération de prix ---
    static std::atomic<bool> stopRequested; // Flag atomique pour signaler l'arrêt au thread (thread-safe par nature atomique).
    static std::thread priceGenerationWorker; // L'objet thread qui exécute la boucle de génération de prix.

    // --- Membres statiques pour la dernière valeur de prix SRD-BTC et son mutex ---
    // Le prix est mutable et partagé entre le thread de génération et les threads qui l'appellent (Bot, etc.).
    static std::mutex srdMutex; // Mutex pour PROTÉGER l'accès (lecture/écriture) à lastSRDBTCValue.
    static double lastSRDBTCValue; // La dernière valeur de prix connue - PROTÉGÉE par srdMutex.

    // --- Membres statiques pour le buffer circulaire des prix historiques et son mutex ---
    // Le buffer et son index sont modifiés/lus par le thread de génération et lus par les appelants (Bot::calculateBands par ex).
    static std::vector<double> ActiveSRDBTC; // Buffer circulaire des prix - PROTÉGÉ par bufferMutex.
    static std::atomic<int> activeIndex; // Index de prochaine écriture dans le buffer (utilisation atomique suffisante pour l'index).
    static const int MAX_VALUES_PER_DAY = 5760; // Capacité max du buffer (constant).
    static std::mutex bufferMutex; // Mutex pour PROTÉGER l'accès (lecture/écriture) au buffer ActiveSRDBTC et à l'index si atomic ne suffit pas pour certaines opérations (ici, l'index est atomique, le mutex protège le vecteur).


    // --- Méthodes privées (implémentations internes) ---
    // Callback pour recevoir les données (ex: depuis une requête réseau, typique de libcurl) - DOIT être statique.
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    // Fonction exécutée dans le thread de génération de prix.
    static void generate_SRD_BTC_loop_impl();

public:
    // --- Méthodes de gestion du thread de génération de prix ---
    static void startPriceGenerationThread(); // Démarre le thread.
    static void stopPriceGenerationThread(); // Signale l'arrêt et attend la fin du thread.

    // --- Méthodes d'accès aux prix (DOIVENT être thread-safe dans .cpp) ---
    // L'implémentation de ces méthodes doit utiliser les mutex (srdMutex et bufferMutex)
    // pour s'assurer que l'accès aux données partagées est sécurisé.
    static double getPrice(const std::string& currency); // Obtient dernier prix (Thread-safe)
    static double getPreviousPrice(const std::string& currency, int secondsBack); // Obtient prix historique (Thread-safe)

    // --- Méthodes d'accès aux flags (pour vérifier l'état global) ---
    static std::atomic<bool>& getStopRequested(); // Retourne une référence au flag d'arrêt (accès atomique thread-safe).
};

#endif // GLOBAL_H