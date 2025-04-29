#include "../headers/TransactionQueue.h"
// Inclure BotSession.h car TransactionQueue interagit avec elle (via shared_ptr/weak_ptr)
#include "../headers/BotSession.h"
// Inclure Logger.h pour le logging
#include "../headers/Logger.h"

#include <iostream>     
#include <chrono>       
#include <utility>      
#include <queue>        
#include <mutex>        
#include <condition_variable> 
#include <thread>       
#include <string>       
#include <memory>      


// Déclaration de la file de transactions globale
extern TransactionQueue txQueue;


// --- Constructeur de TransactionQueue ---
// Initialise le flag running à false.
TransactionQueue::TransactionQueue() : running(false) {
    // Utilisation de la macro LOG avec string literal pour le niveau
    LOG("TransactionQueue créée.", "DEBUG");
}

// --- Destructeur de TransactionQueue ---
// S'assure que le thread worker est arrêté proprement.
TransactionQueue::~TransactionQueue() {
    // Utilisation de la macro LOG avec string literal pour le niveau
    LOG("Destructeur TransactionQueue appelé. Arrêt du thread de traitement...", "DEBUG");
    stop(); // Appelle la méthode stop() pour signaler l'arrêt et joindre le thread
    // Utilisation de la macro LOG avec string literal pour le niveau
    LOG("Destructeur TransactionQueue terminé.", "DEBUG");
}

// --- Enregistre une BotSession auprès de la file de transactions ---
// La file a besoin d'accéder aux sessions pour leur appliquer les transactions.
// On stocke un weak_ptr pour éviter une référence circulaire qui empêcherait BotSession d'être détruite.
// Appelée par le Server lorsqu'une nouvelle session est créée.
void TransactionQueue::registerSession(const std::shared_ptr<BotSession>& session) {
    // Vérifie si le pointeur partagé vers la session est valide
    if (!session) {
        // Utilisation de la macro LOG avec string literal pour le niveau
        LOG("[TransactionQueue::registerSession] Erreur : Tentative d'enregistrer une session null.", "ERROR");
        return; // Sort si la session est nulle
    }
    std::lock_guard<std::mutex> lock(sessionMapMtx); // Protège l'accès concurrentiel à la map sessionMap
    // Stocke un weak_ptr vers la session dans la map, indexé par l'ID du client.
    // La conversion de shared_ptr en weak_ptr est implicite.
    sessionMap[session->getId()] = session;
    // Utilisation de la macro LOG avec string literal pour le niveau
    LOG("[TransactionQueue::registerSession] Session enregistrée pour client ID: " + session->getId(), "INFO");
}

// --- Désenregistre une BotSession de la file de transactions ---
// Supprime l'entrée de la map.
// Appelée par le Server ou le thread HandleClient lorsque la session est terminée.
void TransactionQueue::unregisterSession(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(sessionMapMtx); // Protège l'accès concurrentiel à la map sessionMap
    auto it = sessionMap.find(clientId); // Cherche l'entrée pour cet ID client
    if (it != sessionMap.end()) {
        // Supprime l'entrée de la map. Le weak_ptr est détruit.
        sessionMap.erase(it);
        // Utilisation de la macro LOG avec string literal pour le niveau
        LOG("[TransactionQueue::unregisterSession] Session désenregistrée pour client ID: " + clientId, "INFO");
    } else {
        // L'entrée n'a pas été trouvée (déjà désenregistrée ou ID incorrect).
        // Utilisation de la macro LOG avec string literal pour le niveau
        LOG("[TransactionQueue::unregisterSession] Session pour client ID: " + clientId + " non trouvée dans sessionMap (déjà désenregistrée ?).", "WARNING");
    }
}


// --- Ajoute une requête de transaction à la file d'attente ---
// Appelée par les Bots lorsqu'ils veulent effectuer une transaction.
// Thread-safe.
void TransactionQueue::addRequest(const TransactionRequest& request) {
    { // Bloc pour limiter la portée du lock_guard
        std::lock_guard<std::mutex> lock(mtx); // Protège l'accès concurrentiel à la queue
        queue.push(request); // Ajoute la requête à la fin de la queue
        // LOG("[TransactionQueue::addRequest] Requête ajoutée pour client ID: " + request.clientId + ". Taille de la queue: " + std::to_string(queue.size()), "DEBUG"); // Utilisation correcte de LOG (trop verbeux si décommenté)
    } // Le verrou est libéré ici
    cv.notify_one(); // Notifie un thread (ici, le thread worker) qu'une requête est disponible
}

// --- Démarre le thread de traitement des transactions ---
// Appelée par le Server au démarrage.
void TransactionQueue::start() {
    // Utilise load() pour lire le flag atomique de manière thread-safe
    if (!running.load(std::memory_order_acquire)) { // Vérifie si le thread n'est pas déjà démarré
        running.store(true, std::memory_order_release); // Positionne le flag à true (thread-safe)
        // Crée le thread worker qui exécutera la méthode process()
        // 'this' est passé pour que process() s'exécute sur l'instance courante de TransactionQueue.
        worker = std::thread(&TransactionQueue::process, this);
        // Utilisation de la macro LOG avec string literal pour le niveau
        LOG("[TransactionQueue::start] Thread de traitement démarré.", "INFO");
    } else {
        // Le thread semble déjà en cours
        // Utilisation de la macro LOG avec string literal pour le niveau
        LOG("[TransactionQueue::start] Thread de traitement déjà en cours.", "WARNING");
    }
}

// --- Demande l'arrêt du thread de traitement et attend sa fin ---
// Appelée par le Server à l'arrêt (typiquement dans StopServer ou le destructeur).
// Thread-safe.
void TransactionQueue::stop() {
    // Utilise load() pour vérifier l'état atomique de manière thread-safe
    if (running.load(std::memory_order_acquire)) { // Vérifie si le thread est en cours
        { // Bloc pour limiter la portée du lock_guard
            std::lock_guard<std::mutex> lock(mtx); // Protège l'accès concurrentiel au flag running et potentiellement à la queue
            running.store(false, std::memory_order_release); // Positionne le flag d'arrêt à false (thread-safe)
        } // Le verrou est libéré ici
        cv.notify_all(); // Notifie *tous* les threads en attente (juste le worker ici) qu'ils doivent se réveiller et vérifier le flag running.
        // Attend que le thread worker termine son exécution. C'est une jointure (join).
        // priceGenerationWorker a été créé sans detach() pour pouvoir le joindre ici.
        if (worker.joinable()) { // Vérifie si le thread peut être joint
            worker.join(); // Bloque l'exécution jusqu'à ce que le thread 'worker' soit terminé
            // Utilisation de la macro LOG avec string literal pour le niveau
             LOG("[TransactionQueue::stop] Thread de traitement joint.", "INFO");
        } else {
             // Le thread n'était pas joignable (non démarré, déjà terminé, ou déjà joint/détaché par erreur ailleurs)
            // Utilisation de la macro LOG avec string literal pour le niveau
             LOG("[TransactionQueue::stop] Thread de traitement non joignable.", "WARNING");
        }
    } else {
        // Le thread semble déjà arrêté ou n'a jamais été démarré
        // Utilisation de la macro LOG avec string literal pour le niveau
        LOG("[TransactionQueue::stop] Thread de traitement déjà arrêté ou non démarré.", "WARNING");
    }
    // Le thread 'worker' est maintenant terminé et nettoyé par join().
}

// --- Fonction principale du thread worker ---
// Cette fonction est exécutée par le thread 'worker' de TransactionQueue.
// Elle tourne en boucle, attendant les requêtes et les traitant.
void TransactionQueue::process() {
    // Utilisation de la macro LOG avec string literal pour le niveau
    LOG("[TransactionQueue::process] Thread de traitement en cours...", "INFO");

    // Boucle principale du thread worker
    while (true) {
        // --- Section critique : accès à la queue et attente ---
        std::unique_lock<std::mutex> lock(mtx); // Accquiert le verrou sur le mutex.

        // Attend sur la variable de condition (bloquant).
        // Le thread se réveille quand :
        // 1. La file n'est pas vide (!queue.empty())
        // 2. Le flag running est false (!running.load())
        cv.wait(lock, [&] {
            // Conditions pour sortir de l'attente :
            // - Il y a des requêtes à traiter (!queue.empty())
            // - L'arrêt est demandé (!running.load())
            return !queue.empty() || !running.load(std::memory_order_acquire);
        });

        // --- Vérification de la condition de sortie de la boucle principale ---
        // Si le flag running est false (l'arrêt a été demandé) ET la queue est vide (toutes les requêtes ont été traitées),
        // alors le thread doit s'arrêter.
        if (!running.load(std::memory_order_acquire) && queue.empty()) {
            // Utilisation de la macro LOG avec string literal pour le niveau
            LOG("[TransactionQueue::process] Conditions d'arrêt atteintes. Sortie de la boucle de traitement.", "INFO");
            break; // Sort de la boucle while(true)
        }

        // --- Traitement de la requête (si la queue n'est pas vide) ---
        // Si on arrive ici, soit running est true et la queue s'est remplie, soit running est false
        // mais il restait des éléments dans la queue à traiter.
        // On traite une requête *uniquement* si la queue n'est pas vide.
        if (!queue.empty()) { // Double vérification, mais le wait/break gère déjà le cas queue.empty() && !running
                                // Si on est ici, c'est nécessairement parce que !queue.empty() est true (ou running était false avec queue non vide).

            // Récupère la requête de la file.
            // Déclare et initialise la variable 'req' ICI, en la copiant (ou déplaçant) directement depuis la queue.
            TransactionRequest req = queue.front(); // <-- Déclare et initialise 'req' ici. Ceci utilise le constructeur de copie implicite (ou par défaut).
                                                    // LA LIGNE "TransactionRequest req;" PLUS HAUT (avant le while) DOIT ÊTRE RETIREE.
            queue.pop(); // Retire l'élément traité de la queue

            // Libère le verrou dès que possible pour permettre à d'autres threads d'ajouter des requêtes pendant le traitement
            lock.unlock();

            // --- Le reste du traitement utilise l'objet 'req' ---
            // Note : Utilise 'req' pour accéder aux membres (req.clientId, req.type, etc.)
            LOG("[TransactionQueue::process] Traitement requête pour client ID: " + req.clientId + ", Type: " + (req.type == RequestType::BUY ? "BUY" : "SELL") + ", Quantité: " + std::to_string(req.quantity), "DEBUG");

            // Chercher la session correspondante
            std::shared_ptr<BotSession> session = nullptr;
            {
                std::lock_guard<std::mutex> sessionLock(sessionMapMtx);
                auto it = sessionMap.find(req.clientId); // Utilise req.clientId
                if (it != sessionMap.end()) {
                    session = it->second.lock();
                }
            }

            if (session) {
                try {
                    session->applyTransactionRequest(req); // Utilise req
                } catch (const std::exception& e) {
                    LOG("[TransactionQueue::process] Exception lors de l'application de la transaction pour client ID: " + req.clientId + " : " + std::string(e.what()), "ERROR"); // Utilise req.clientId
                }

            } else {
                LOG("[TransactionQueue::process] BotSession introuvable ou invalide pour client ID: " + req.clientId + ". Requête ignorée.", "WARNING"); // Utilise req.clientId
            }
            // --- Fin Traitement de la requête ---
        } // Fin if (!queue.empty())
    } // Fin while(true) de la boucle principale du thread worker

    // Le thread worker atteint cette ligne après être sorti de la boucle while(true).
    // Utilisation de la macro LOG avec string literal pour le niveau
    LOG("[TransactionQueue::process] Thread de traitement terminé.", "INFO");
}