#include "../headers/Global.h"
#include "../headers/Logger.h" 


#include <curl/curl.h>      
#include <nlohmann/json.hpp> 
#include <iostream>         
#include <fstream>          
#include <vector>           
#include <ctime>            
#include <atomic>         
#include <random>         
#include <thread>          
#include <chrono>         
#include <cmath>           
#include <filesystem>       
#include <stdexcept>        
#include <iomanip>          
#include <sstream>          


// Utilisation des espaces de noms pour la bibliothèque JSON
using json = nlohmann::json;

// --- Initialisation des membres statiques de la classe Global ---

std::mutex Global::srdMutex;
double Global::lastSRDBTCValue = 0.0;

// Initialisation du buffer circulaire avec la taille correcte (Correction de la faute de frappe)
std::vector<double> Global::ActiveSRDBTC(Global::MAX_VALUES_PER_DAY, 0.0);
std::atomic<int> Global::activeIndex = 0;
std::atomic<bool> Global::stopRequested = false;

// Initialisation du membre statique thread
std::thread Global::priceGenerationWorker;


// --- Implémentation de la callback de libcurl ---

// Cette fonction est appelée par libcurl pour gérer les données reçues.
size_t Global::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    // userp est un pointeur vers le std::string où stocker les données
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb; // Retourne le nombre d'octets traités
}


// --- Implémentation de la fonction de boucle de génération de prix ---

// Cette fonction s'exécute dans le thread priceGenerationWorker.
void Global::generate_SRD_BTC_loop_impl() {
    // Le thread tourne tant que le flag d'arrêt n'est pas positionné
    LOG("[Global] Thread de génération de prix démarré.", "INFO");

    // --- Initialisation des ressources utilisées pendant la vie du thread ---

    // Handle CURL pour les requêtes HTTP(S). Initialisé UNE SEULE FOIS.
    CURL* curl = nullptr; 

    // Buffer pour stocker la réponse CURL. Nettoyé à chaque itération.
    std::string readBuffer; 

    // Générateur aléatoire pour la simulation de prix. Initialisé UNE SEULE FOIS.
    std::default_random_engine generator(std::random_device{}());
    std::normal_distribution<double> distribution(0.0, 0.015); 


    // Chemin hardcodé pour le fichier de log des prix
    std::string priceLogPath = "../src/data/srd_btc_values.csv";

    // Tenter d'ouvrir/créer le fichier de log des prix en mode append (ajout à la fin).
    // Initialisé UNE SEULE FOIS.
    std::ofstream priceFile(priceLogPath, std::ios::app);
    if (!priceFile.is_open()) {
        LOG("[Global] Impossible d'ouvrir/créer le fichier de log des prix : " + priceLogPath + ". Le thread continuera mais sans logging disque.", "ERROR");
        // Le thread peut continuer à fonctionner, mais les prix ne seront pas loggués sur disque.
    } else {
        // Si le fichier est vide, ajouter l'en-tête CSV.
        priceFile.seekp(0, std::ios::end); // Se positionne à la fin du fichier.
        if (priceFile.tellp() == 0) { // Si la position est 0, le fichier était vide.
             priceFile << "Timestamp,SRD-BTC_USD\n";
             priceFile.flush(); // Assure que l'en-tête est écrit immédiatement sur disque.
             LOG("[Global] Fichier de log des prix '" + priceLogPath + "' ouvert. En-tête ajouté.", "INFO");
        } else {
             LOG("[Global] Fichier de log des prix '" + priceLogPath + "' ouvert en mode append.", "INFO");
        }
    }

    // --- Initialisation du handle cURL UNE SEULE FOIS avant d'entrer dans la boucle ---
    // Tenter l'initialisation de curl_easy_init() une seule fois.
    curl = curl_easy_init(); 
    if (!curl) {
        // Si l'initialisation échoue, logguer une erreur critique.
        // Le thread continuera à tourner (la boucle while ne dépend pas de 'curl'),
        // mais les requêtes API échoueront dans la boucle car 'curl' sera nullptr.
        LOG("[Global] Erreur fatale : Impossible d'initialiser cURL pour le thread de génération de prix. Les requêtes API échoueront.", "ERROR");
    }

    // --- Boucle principale du thread de génération de prix ---
    // Le thread s'exécute tant que stopRequested.load() est false.
    while (!stopRequested.load()) { 
        
        // --- Début : Récupération et simulation du prix SRD-BTC ---

        double currentBTCValue = -1.0; // Réinitialiser la valeur BTC pour chaque cycle.

        // --- Effectuer la requête API si le handle cURL est valide ---
        if (curl) { // Utilise le handle 'curl' qui a été initialisé AVANT la boucle
            readBuffer.clear(); // Nettoyer le buffer pour stocker la réponse JSON de CETTE requête.
            
            // Configurer les options de la requête pour cette itération.
            curl_easy_setopt(curl, CURLOPT_URL, "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // Callback pour lire la réponse
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer); // Buffer où la callback doit écrire
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Timeout pour la requête

            // Exécuter la requête en utilisant le handle cURL réutilisé.
            CURLcode res = curl_easy_perform(curl); 

            if (res == CURLE_OK) {
                // La requête HTTP(S) a réussi.
                try {
                    // Parser la réponse JSON reçue dans readBuffer.
                    auto jsonData = json::parse(readBuffer);
                    // Tenter d'extraire le prix du Bitcoin en USD.
                    if (jsonData.contains("bitcoin") && jsonData["bitcoin"].contains("usd")) {
                        currentBTCValue = jsonData["bitcoin"]["usd"].get<double>(); // Convertir la valeur JSON en double.
                         LOG("[Global] Prix BTC/USD récupéré via API : " + std::to_string(currentBTCValue), "DEBUG"); 
                    } else {
                         LOG("[Global] Réponse JSON de CoinGecko inattendue (pas de chemin bitcoin.usd). Réponse: '" + readBuffer + "'. Socket FD: N/A", "WARNING"); // Logguer la réponse complète inattendue
                    }
                } catch (const json::exception& e) {
                    // Gérer les erreurs de parsing JSON.
                    LOG("[Global] Erreur parsing JSON de la réponse CoinGecko : " + std::string(e.what()) + ". Réponse brute: '" + readBuffer + "'. Socket FD: N/A", "ERROR"); 
                } catch (const std::exception& e) {
                     // Gérer d'autres exceptions potentielles pendant le traitement de la réponse.
                     LOG("[Global] Erreur inattendue lors du traitement de la réponse CoinGecko : " + std::string(e.what()) + ". Socket FD: N/A", "ERROR"); 
                }
            } else {
                // La requête cURL a échoué.
                LOG("[Global] Erreur cURL lors de la récupération du prix : " + std::string(curl_easy_strerror(res)) + ". Socket FD: N/A", "ERROR"); 
                // Gérer l'échec : le prix currentBTCValue reste -1.0 (invalide), ce qui sera géré plus bas.
            }

        } else {
             // Le handle cURL n'a pas pu être initialisé au démarrage du thread.
             // Logguer une erreur si le thread n'est pas déjà en train de s'arrêter.
             if (!stopRequested.load()) {
                 LOG("[Global] Le handle cURL n'est pas valide. Impossible d'effectuer la requête API. Socket FD: N/A", "ERROR");
             }
             // currentBTCValue reste -1.0, ce qui sera géré plus bas.
        }
        // --- Fin de la récupération du prix BTC ---


        // Si un prix BTC valide a été récupéré (currentBTCValue > 0).
        if (currentBTCValue > 0) {
            // --- Simulation du prix SRD-BTC basé sur le prix BTC réel ---
            // Appliquer une petite fluctuation aléatoire au prix BTC réel récupéré.
            double fluctuation = distribution(generator);
           
            double srd_btc = currentBTCValue*(1.0 + fluctuation); // Le prix SRD-BTC simulé est le prix BTC + fluctuation.

            // S'assurer que le prix reste positif (même si la simulation donne une très légère valeur négative).
            if (srd_btc <= 0) srd_btc = 0.01; // Définir une petite valeur minimale positive si nécessaire.

            // --- Mettre à jour la dernière valeur SRD-BTC globale (thread-safe) ---
            // Utiliser un mutex pour synchroniser l'accès à lastSRDBTCValue
            {
                std::lock_guard<std::mutex> lock(srdMutex); // Aquiert le verrou sur srdMutex.
                lastSRDBTCValue = srd_btc; // Mettre à jour la valeur.
            } // Le verrou est automatiquement libéré ici.

            // --- Mettre à jour le buffer circulaire historique (thread-safe) ---
            // activeIndex est un std::atomic.
            // Stocker la nouvelle valeur srd_btc dans le buffer ActiveSRDBTC à l'index courant.
            int index = activeIndex.load(std::memory_order_relaxed); // Lecture atomique de l'index courant.
            ActiveSRDBTC[index] = srd_btc; // Stocker la nouvelle valeur dans le tableau ou vector.
            // Incrémenter l'index de manière circulaire et atomique.
            // Utiliser memory_order_release pour s'assurer que l'écriture dans ActiveSRDBTC est visible
            // avant la mise à jour de l'index.
            activeIndex.store((index + 1) % MAX_VALUES_PER_DAY, std::memory_order_release); 


            // --- Logging de la nouvelle valeur simulée ---
            // Obtenir le timestamp actuel pour le log et le fichier.
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            // Convertir en struct tm en utilisant la version thread-safe (localtime_r sur POSIX, ou équivalent).
            std::tm timeinfo_buffer;
            // localtime_r renvoie un pointeur vers timeinfo_buffer, qui doit être passé à put_time.
            std::tm* timeinfo = localtime_r(&time_t, &timeinfo_buffer); // Utilise localtime_r pour la thread-safety

            std::stringstream ss_timestamp;
            if (timeinfo) {
                // Formater le timestamp en chaîne (ex: "YYYY-MM-DD HH:MM:SS").
                ss_timestamp << std::put_time(timeinfo, "%Y-%m-%d %X"); 
            } else {
                 ss_timestamp << "[TIMESTAMP_ERROR]"; // Gérer le cas où localtime_r échoue.
            }

            // Logguer la nouvelle valeur SRD-BTC simulée.
            LOG("[Global] Nouvelle valeur SRD-BTC : " + std::to_string(srd_btc) + " USD à " + ss_timestamp.str(), "INFO"); 

            // --- Écrire la valeur simulée dans le fichier de log des prix ---
            // Vérifier si le fichier de log est ouvert avant d'écrire.
            if (priceFile.is_open()) {
                // Écrire au format CSV : Timestamp,Valeur
                // Utiliser std::fixed et std::setprecision pour contrôler le format d'affichage des nombres à virgule flottante.
                priceFile << ss_timestamp.str() << "," << std::fixed << std::setprecision(10) << srd_btc << "\n"; 
                // Optionnel : Flusher le buffer d'écriture pour s'assurer que les données sont écrites sur le disque immédiatement.
                // Cela peut réduire la perte de données en cas de crash, mais peut impacter les performances si appelé très fréquemment.
                priceFile.flush(); 
            } else {
                 // Si le fichier n'est pas ouvert, l'erreur a déjà été logguée lors de l'ouverture.
                 // On ne peut pas logguer cette écriture spécifique.
            }
            // --- Fin Simulation & Logging ---

        } else {
             // Si currentBTCValue était <= 0 (par ex., échec de la récupération API ou parsing).
             // Logguer un avertissement pour indiquer que la valeur n'a pas été mise à jour dans ce cycle.
             LOG("[Global] Prix BTC non récupéré ou invalide (" + std::to_string(currentBTCValue) + "). La valeur SRD-BTC n'est pas mise à jour dans ce cycle. Socket FD: N/A", "WARNING"); 
        }

        // --- Pause entre les cycles de mise à jour ---
        // C'est crucial pour contrôler la fréquence des requêtes API et des mises à jour.
        // MAX_VALUES_PER_DAY (si utilisé comme le nombre de valeurs *par jour*) doit être cohérent avec cette pause.
        // 86400 secondes dans une journée. Si pause = 15s, 86400/15 = 5760 valeurs par jour.
        std::this_thread::sleep_for(std::chrono::seconds(15)); // Pause pour contrôler la fréquence de mise à jour.

    } // --- Fin de la boucle while (!stopRequested.load()) ---

    // --- Nettoyage final des ressources allouées par ce thread avant qu'il ne se termine ---
    // Cette partie s'exécute uniquement lorsque la condition de la boucle while devient fausse, c'est-à-dire quand stopRequested.load() est vrai.

    // Nettoyer le handle cURL si il a été initialisé avec succès au moins une fois.
    if (curl) { 
        curl_easy_cleanup(curl); // Nettoyer le handle cURL alloué par curl_easy_init().
        LOG("[Global] Handle cURL nettoyé avant l'arrêt du thread de génération de prix.", "DEBUG");
    }

    // Fermer le fichier de log des prix si il a été ouvert avec succès.
    if (priceFile.is_open()) {
        // Écrire une marque dans le fichier de log pour indiquer la fin de cette session de log.
        // Obtenir le timestamp actuel.
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm timeinfo_buffer;
        std::tm* timeinfo = localtime_r(&time_t, &timeinfo_buffer); 

        std::stringstream ss_timestamp;
         if (timeinfo) {
            ss_timestamp << std::put_time(timeinfo, "%Y-%m-%d %X"); 
        } else {
             ss_timestamp << "[TIMESTAMP_ERROR]";
        }

        priceFile << "--- Fin du log de prix : " << ss_timestamp.str() << " ---\n";
        priceFile.close(); // Fermer le fichier de log des prix.
        LOG("[Global] Fichier de log des prix '" + priceLogPath + "' fermé.", "INFO");
    }

    // Logguer que le thread de génération de prix s'est terminé proprement.
    LOG("[Global] Thread de génération de prix terminé.", "INFO"); 
    
    // Les autres ressources (generator, readBuffer, etc.) seront automatiquement libérées à la fin de la fonction (quand elles sortent de portée).
}


// --- Implémentation des méthodes de gestion du thread de prix ---

// Méthode statique pour démarrer le thread interne de Global
void Global::startPriceGenerationThread() {
    // Vérifie si le thread est joignable (s'il a déjà été créé et n'a pas été joint/détaché)
    // Si le thread est déjà en cours, ne fais rien.
    if (!priceGenerationWorker.joinable()) {
        LOG("[Global] Demande de démarrage du thread de génération de prix.", "INFO");
        // S'assurer que le flag d'arrêt est false avant de démarrer le thread
        stopRequested.store(false); // Initialise le flag d'arrêt

        // Crée un nouveau thread qui exécutera la fonction generate_SRD_BTC_loop_impl
        // Le thread est stocké dans le membre statique priceGenerationWorker.
        priceGenerationWorker = std::thread(&Global::generate_SRD_BTC_loop_impl);

        LOG("[Global] Thread de génération de prix initié.", "INFO");
    } else {
        // Le thread semble déjà lancé
        LOG("[Global] Thread de génération de prix déjà en cours (startPriceGenerationThread ignoré).", "WARNING");
    }
}

// Méthode statique pour signaler l'arrêt et joindre le thread interne de Global
void Global::stopPriceGenerationThread() {
    LOG("[Global] Demande d'arrêt du thread de génération de prix.", "INFO");
    // Positionne le flag atomique pour signaler au thread de s'arrêter
    stopRequested.store(true);

    // Attend que le thread se termine. C'est une jointure (join).
    // C'est pour cela que le thread n'est pas détaché dans startPriceGenerationThread.
    if (priceGenerationWorker.joinable()) { // Vérifie si le thread peut être joint
        LOG("[Global] Jointure du thread de génération de prix...", "INFO");
        priceGenerationWorker.join(); // Bloque jusqu'à ce que le thread se termine
        LOG("[Global] Thread de génération de prix joint.", "INFO");
    } else {
        // Le thread n'était pas joignable (non démarré, déjà terminé, ou déjà joint/détaché par erreur ailleurs)
        LOG("[Global] Thread de génération de prix non joignable ou déjà terminé lors de la demande d'arrêt.", "WARNING"); 
    }
    // Le thread 'priceGenerationWorker' est maintenant terminé et nettoyé par join().
}


// --- Implémentation des méthodes d'accès aux prix ---

// Retourne le dernier prix SRD-BTC connu (thread-safe)
double Global::getPrice(const std::string& currency) {
    // Seul SRD-BTC est supporté par cette simulation.
    if (currency != "SRD-BTC") {
        return 0.0; // Retourne 0 pour une devise non supportée
    }
    // Utilise un lock_guard pour accéder à la valeur partagée lastSRDBTCValue de manière thread-safe.
    std::lock_guard<std::mutex> lock(srdMutex);
    return lastSRDBTCValue; // Retourne la dernière valeur mise à jour
}

// Retourne un prix historique approximatif en utilisant le buffer circulaire (thread-safe)
// 'secondsBack' indique le nombre de secondes dans le passé.
// Le buffer stocke 24h de données avec une mise à jour toutes les 30 secondes.
double Global::getPreviousPrice(const std::string& currency, int secondsBack) {
     if (currency != "SRD-BTC") {
        return 0.0;
     }
     // secondsBack doit être un nombre positif de secondes.
     if (secondsBack <= 0) {
        return getPrice(currency); // Retourne le prix actuel si 0 ou négatif
     }

    // Calculer le nombre de pas en arrière dans le buffer circulaire. Chaque pas correspond à la période entre deux mises à jour.
    int stepsBack = secondsBack / 15; // Suppose une mise à jour toutes les 15 secondes

    // Si le nombre de pas dépasse la taille du buffer, limite à la taille max moins 1
    // (pour accéder au plus vieil élément disponible).
    if (stepsBack >= MAX_VALUES_PER_DAY) {
        LOG("[Global] getPreviousPrice - Histoire demandée (" + std::to_string(secondsBack) + "s) excède la capacité du buffer (" + std::to_string(MAX_VALUES_PER_DAY * 2) + "s). Retourne le plus vieil élément.", "WARNING"); // Utilisation correcte de LOG
        stepsBack = MAX_VALUES_PER_DAY - 1; // Utilise l'index du plus vieil élément stocké
    }

    // Calculer l'index cible dans le buffer circulaire.
    // L'index courant (`activeIndex`) pointe vers la case où le prochain élément sera écrit.
    // Pour trouver l'élément `stepsBack` en arrière, on part de l'index courant, on soustrait `stepsBack`.
    // Le modulo `% MAX_VALUES_PER_DAY` gère l'enroulement du buffer.
    int raw_index = activeIndex.load(std::memory_order_acquire) - stepsBack; // Lecture atomique (acquire pour synchronisation avec l'écriture store)
    // Gérer l'indice négatif : si raw_index est négatif, on ajoute MAX_VALUES_PER_DAY pour obtenir le bon index circulaire.
    int target_index = raw_index % MAX_VALUES_PER_DAY;
    if (target_index < 0) {
        target_index += MAX_VALUES_PER_DAY;
    }

    // Accéder à la valeur dans le buffer.
    // L'accès en lecture au buffer n'est pas protégé par un mutex.
    // On se fie à la lecture atomique de l'index et au fait que l'écriture du double est atomique sur la plupart des architectures modernes pour les types alignés.
    return ActiveSRDBTC[target_index]; // Retourne la valeur historique
}

// Méthode statique pour accéder au flag d'arrêt (utilisée par la boucle interne generate_SRD_BTC_loop_impl)
std::atomic<bool>& Global::getStopRequested() {
    return stopRequested; // Retourne la référence atomique
}