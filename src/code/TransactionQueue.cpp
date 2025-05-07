
#include "../headers/TransactionQueue.h"
#include "../headers/ClientSession.h" // Pour accéder à ClientSession et son Wallet
#include "../headers/Logger.h" 
#include "../headers/Global.h" 
#include "../headers/Transaction.h"


// Includes pour les fonctionnalités standards :
#include <iostream>
#include <chrono>
#include <utility>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <string>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cctype>


// --- Implémentation de requestTypeToString ---
// Convertit une RequestType enum en string.
std::string requestTypeToString(RequestType type) {
    switch (type) {
        case RequestType::BUY: return "BUY";
        case RequestType::SELL: return "SELL";
        case RequestType::UNKNOWN_REQUEST: return "UNKNOWN_REQUEST";
        default: return "UNKNOWN_REQUEST"; // Devrait pas arriver avec les types définis
    }
}


// --- Initialisation des membres NON statiques ---
// Ils sont initialisés dans le constructeur.


// --- Constructeur de TransactionQueue ---
// Initialise le flag running à false.
TransactionQueue::TransactionQueue() : running(false) {
}

// --- Destructeur de TransactionQueue ---
// Assure que le thread worker est arrêté proprement.
TransactionQueue::~TransactionQueue() {
    stop(); // Appelle la méthode stop()
}

// --- Enregistre une ClientSession ---
// Stocke un weak_ptr vers la session dans sessionMap. Thread-safe.
void TransactionQueue::registerSession(const std::shared_ptr<ClientSession>& session) {
    if (!session) {
        LOG("TransactionQueue::registerSession Erreur : Tentative d'enregistrer une session null.", "ERROR");
        return;
    }
    std::lock_guard<std::mutex> lock(sessionMapMtx);
    sessionMap[session->getClientId()] = session; // session->getId() est thread-safe (const)
    LOG("TransactionQueue::registerSession Session enregistrée pour client ID: " + session->getClientId(), "INFO");
}

// --- Désenregistre une ClientSession ---
// Supprime l'entrée de sessionMap. Thread-safe.
void TransactionQueue::unregisterSession(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(sessionMapMtx);
    auto it = sessionMap.find(clientId);
    if (it != sessionMap.end()) {
        sessionMap.erase(it);
        LOG("TransactionQueue::unregisterSession Session désenregistrée pour client ID: " + clientId, "INFO");
    } else {
        LOG("TransactionQueue::unregisterSession Session pour client ID: " + clientId + " non trouvée dans sessionMap lors du désenregistrement.", "WARNING");
    }
}

// --- Implémentation de start ---
// Démarre le thread de traitement de la file. Appelée par le Server au démarrage.
void TransactionQueue::start() {
    // Vérifie si le thread n'est pas déjà en cours et s'il n'est pas joignable.
    if (!running.load(std::memory_order_acquire) && !worker.joinable()) {
        running.store(true, std::memory_order_release); // Indique que la file doit tourner
        worker = std::thread(&TransactionQueue::process, this); // Lance le thread worker sur la méthode process
        LOG("TransactionQueue::start Thread de traitement démarré.", "INFO");
    } else {
        LOG("TransactionQueue::start Thread de traitement déjà en cours ou non démarrable.", "WARNING");
    }
}

// --- Demande l'arrêt du thread et attend sa fin ---
// Appelée par le Server ou le destructeur. Thread-safe.
void TransactionQueue::stop() {
    if (running.load(std::memory_order_acquire) || worker.joinable()) {
        { // Section critique pour running flag
            std::lock_guard<std::mutex> lock(mtx);
            running.store(false, std::memory_order_release);
        } // Le verrou est libéré

        cv.notify_all(); // Réveille le worker

        if (worker.joinable()) {
            worker.join();
             LOG("TransactionQueue::stop Thread de traitement joint.", "INFO");
        } else {
             LOG("TransactionQueue::stop Thread de traitement non joignable.", "WARNING");
        }
    } else {
        LOG("TransactionQueue::stop Thread de traitement déjà arrêté ou non démarré.", "WARNING");
    }
}

// --- Fonction principale du thread worker ---
// Tourne en boucle, attendant et traitant les requêtes.
void TransactionQueue::process() {
    LOG("TransactionQueue::process Thread de traitement en cours...", "INFO");

    while (true) {
        std::unique_lock<std::mutex> lock(mtx);

        cv.wait(lock, [&] {
            return !queue.empty() || !running.load(std::memory_order_acquire);
        });

        // Condition de sortie : arrêt demandé ET queue vide.
        if (!running.load(std::memory_order_acquire) && queue.empty()) {
            LOG("TransactionQueue::process Conditions d'arrêt atteintes. Sortie de la boucle de traitement.", "INFO");
            break;
        }

        // Traitement si queue non vide.
        if (!queue.empty()) {
            TransactionRequest req = queue.front();
            queue.pop();

            lock.unlock(); // Libère le verrou pendant le traitement (potentiellement long)

            LOG("TransactionQueue::process Traitement requête pour client ID: " + req.clientId + ", Type: " + requestTypeToString(req.type) + ", Quantité: " + std::to_string(req.quantity), "INFO");

            // Appelle la méthode interne pour traiter cette requête.
            processRequest(req); // req est passé par référence et modifié ici.

            // Fin Traitement
        }
    }

    LOG("TransactionQueue::process Thread de traitement terminé.", "INFO");
}


// --- Implémentation de processRequest ---
// Prend une requête par const référence, effectue le traitement sous verrou du Wallet,
// met à jour le Wallet, ajoute à l'historique, crée la Transaction finale,
// et notifie la ClientSession.
void TransactionQueue::processRequest(const TransactionRequest& request) {
    // Log d'entrée de la fonction (utile pour voir que la TQ prend la requête)
    LOG("TransactionQueue::processRequest INFO : Début traitement requête ID client: " + request.clientId + ", Type: " + requestTypeToString(request.type) + ", Quantité: " + std::to_string(request.quantity), "INFO");

    // Variables pour les résultats du traitement
    std::string transactionId = "";
    TransactionType final_tx_type = TransactionType::UNKNOWN;
    double unitPrice = 0.0;
    double totalAmount = 0.0;
    double fee = 0.0; // TODO: Calculer les frais réels si nécessaire
    TransactionStatus status = TransactionStatus::PENDING; // Commence à PENDING, statut final déterminé sous verrou
    std::string failureReason = "";

    // Déclaration du shared_ptr pour la transaction finale.
    // Sera nullptr si la transaction échoue avant la création de l'objet Transaction sous verrou.
    std::shared_ptr<Transaction> final_transaction_ptr = nullptr;


    // --- Génération ID et Timestamp ---
    transactionId = Transaction::generateNewIdString(); // Génère un ID unique pour cette requête/transaction
    auto timestamp = std::chrono::system_clock::now(); // Timestamp actuel
    std::time_t timestamp_t = std::chrono::system_clock::to_time_t(timestamp);


    // --- Préliminaires (accès session/wallet) ---
    std::shared_ptr<ClientSession> session = nullptr;
    std::shared_ptr<Wallet> wallet = nullptr;

    { // Verrou pour accéder à sessionMap
        std::lock_guard<std::mutex> sessionLock(sessionMapMtx);
        auto it = sessionMap.find(request.clientId);
        if (it != sessionMap.end()) {
            session = it->second.lock(); // Tente d'obtenir un shared_ptr
            if (session) {
                wallet = session->getClientWallet(); // getClientWallet() doit retourner shared_ptr<Wallet>
            }
        }
    } // Verrou sessionMap libéré


    // --- Vérification préliminaire Session/Wallet ---
    if (!session || !wallet) {
        status = TransactionStatus::FAILED;
        failureReason = "Client session or wallet not available.";
        LOG("TransactionQueue::processRequest ERROR : ClientSession ou Wallet introuvable pour client ID: " + request.clientId + ". Transaction FAILED préliminaire.", "ERROR");
        // Les variables unitPrice, totalAmount, fee restent à 0.0
        // final_tx_type reste UNKNOWN (ou mappé rapidement avant création Transaction FAILED).
        // L'objet Transaction FAILED sera créé APRÈS le grand 'else'.

    } else { // Session et Wallet disponibles : On peut procéder à la logique sous verrou

        // --- Obtention du prix (avant le bloc verrouillé principal) ---
        unitPrice = Global::getPrice("SRD-BTC"); // Assurez-vous que ce symbole est correct
        // Note : Le prix est re-vérifié sous verrou.


        // --- LOGIQUE DE TRAITEMENT CRUCIALE SOUS VERROU DU WALLET ---
        { // Bloc pour le lock_guard sur le Wallet
            std::lock_guard<std::mutex> walletLock(wallet->getMutex()); // VERROUILLAGE RÉUSSI


            // Re-vérifier la validité du prix SOUS LE VERROU
             if (unitPrice <= 0 || !std::isfinite(unitPrice)) {
                 status = TransactionStatus::FAILED;
                 failureReason = "Invalid market price at execution.";
                 LOG("TransactionQueue::processRequest ERROR : Sous verrou Wallet: Prix invalide. Statut FAILED.", "ERROR");
             } else { // Prix valide

                // Calculer les montants réels et vérifier les fonds SOUS LE VERROU
                double amount_to_use_from_balance = 0.0; // Montant dans la devise du solde utilisé
                double quantity_to_trade_crypto = request.quantity; // Quantité de crypto demandée

                if (request.type == RequestType::BUY) {
                    double usd_cost = quantity_to_trade_crypto * unitPrice;
                    fee = usd_cost * 0.0001; // Frais sur le montant USD
                    totalAmount = usd_cost + fee; // Coût total en USD (montant échangé + frais)
                    amount_to_use_from_balance = totalAmount; // On utilise ce totalAmount du solde USD

                    if (wallet->getBalance(Currency::USD) >= amount_to_use_from_balance) { // APPEL POTENTIELLEMENT CRITIQUE wallet->getBalance !
                        // Fonds USD suffisants : La transaction peut être COMPLETED
                        status = TransactionStatus::COMPLETED;
                        LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": BUY validé sous verrou. Fonds USD suffisants.", "INFO");
                    } else {
                        status = TransactionStatus::FAILED;
                        failureReason = "Insufficient USD funds.";
                        LOG("TransactionQueue::processRequest WARNING : Client " + request.clientId + ": Solde USD insuffisant (" + std::to_string(wallet->getBalance(Currency::USD)) + ") pour BUY de " + std::to_string(amount_to_use_from_balance) + " USD. Transaction FAILED.", "WARNING");
                    }

                } else if (request.type == RequestType::SELL) {
                    amount_to_use_from_balance = quantity_to_trade_crypto;
                    totalAmount = quantity_to_trade_crypto * unitPrice;
                    fee = totalAmount * 0.0001;
                    totalAmount -= fee;

                    if (wallet->getBalance(Currency::SRD_BTC) >= amount_to_use_from_balance) { // APPEL POTENTIELLEMENT CRITIQUE wallet->getBalance !
                        status = TransactionStatus::COMPLETED;
                        LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": SELL validé sous verrou. Fonds SRD-BTC suffisants.", "INFO");
                    } else {
                        status = TransactionStatus::FAILED;
                        failureReason = "Insufficient SRD-BTC funds.";
                        LOG("TransactionQueue::processRequest WARNING : Client " + request.clientId + ": Solde SRD-BTC insuffisant (" + std::to_string(wallet->getBalance(Currency::SRD_BTC)) + ") pour SELL de " + std::to_string(amount_to_use_from_balance) + " SRD-BTC. Transaction FAILED.", "WARNING");
                    }
                } else { // Type non supporté
                    status = TransactionStatus::FAILED;
                    failureReason = "Unsupported or unknown request type.";
                    LOG("TransactionQueue::processRequest ERROR : Sous verrou Wallet: Type de requête non supporté/inconnu.", "ERROR");
                }


                // --- Si la transaction est COMPLETED, mettre à jour les soldes ---
                if (status == TransactionStatus::COMPLETED) {
                    if (request.type == RequestType::BUY) {
                        wallet->updateBalance(Currency::USD, -totalAmount); // APPEL POTENTIELLEMENT CRITIQUE wallet->updateBalance !
                        wallet->updateBalance(Currency::SRD_BTC, request.quantity); // APPEL POTENTIELLEMENT CRITIQUE
                    } else if (request.type == RequestType::SELL) {
                         wallet->updateBalance(Currency::SRD_BTC, -request.quantity); // APPEL POTENTIELLEMENT CRITIQUE wallet->updateBalance !
                         wallet->updateBalance(Currency::USD, totalAmount); // APPEL POTENTIELLEMENT CRITIQUE
                    }

                    wallet->saveToFile(); // APPEL POTENTIELLEMENT CRITIQUE wallet->saveToFile !
                    LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": Wallet sauvegardé après Tx COMPLETED.", "INFO");

                } // Fin if (status == TransactionStatus::COMPLETED)

                // --- Créer l'objet Transaction final et l'ajouter à l'historique ---
                // Ces étapes se font ICI, sous le verrou du Wallet, après les mises à jour.
                // L'objet est créé avec le statut (COMPLETED/FAILED) déterminé ci-dessus.

                 // Mapper RequestType à TransactionType pour l'objet Transaction final.
                 if (request.type == RequestType::BUY) final_tx_type = TransactionType::BUY;
                 else if (request.type == RequestType::SELL) final_tx_type = TransactionType::SELL;
                 else final_tx_type = TransactionType::UNKNOWN; // Cas fallback si type inconnu


                // Construction de l'objet Transaction finale (via make_shared)
                // Les variables transactionId, timestamp_t, status, failureReason, etc.
                // ont toutes été définies à ce point si le grand else { Session/Wallet valides } a été exécuté.
                final_transaction_ptr = std::make_shared<Transaction>( // Construction via make_shared ici, sous verrou !
                    transactionId,      // Utilise l'ID généré plus tôt (avant grand if/else)
                    request.clientId,
                    final_tx_type,      // Type mappé
                    request.cryptoName, // Nom de la crypto demandée
                    request.quantity,   // Quantité demandée
                    unitPrice,          // Prix effectif d'exécution
                    totalAmount,        // Montant total échangé
                    fee,                // Frais
                    timestamp_t,        // Timestamp généré plus tôt
                    status,             // Le statut (COMPLETED/FAILED) déterminé ci-dessus !
                    failureReason       // Raison d'échec si FAILED
                );


                // Ajouter la transaction à l'historique du Wallet.
                wallet->addTransaction(*final_transaction_ptr); // Appel addTransaction SOUS VERROU (avec déréférencement) !
                LOG("TransactionQueue::processRequest INFO : Transaction " + final_transaction_ptr->getId() + " ajoutée à l'historique du Wallet sous verrou pour client " + request.clientId + ". Statut: " + transactionStatusToString(final_transaction_ptr->getStatus()), "INFO");


            } // Fin else (prix valide sous verrou)
            // === FIN DU CONTENU QUI DOIT ETRE DANS CE BLOC VERROUILLÉ ! ===

        } // Le lock_guard walletLock libère le mutex du Wallet ici !

    } // Fin du grand 'else' (Session et Wallet disponibles)


    // === Après le grand 'else' (Session et Wallet disponibles) ===
    // Si Session/Wallet n'étaient PAS disponibles, le bloc ci-dessus est sauté.
    // final_transaction_ptr est toujours nullptr dans ce cas. Il faut quand même créer une transaction FAILED
    // pour le logging global et la notification (si session existe).


    // --- Gérer la Transaction finale (Créer FAILED si nécessaire, puis Log & Notifier) ---
    // Si final_transaction_ptr est nullptr, cela signifie que le grand 'else' (où le Wallet est géré) n'a pas été exécuté,
    // donc la transaction a échoué très tôt (Session/Wallet manquants).
    // On crée ici l'objet Transaction FAILED pour le logging global et la notification si nécessaire.
    if (!final_transaction_ptr) {
        // Les variables de résultat (status, failureReason) ont été réglées dans le bloc if (!session || !wallet)
        // Les variables transactionId, timestamp, timestamp_t ont été générées en haut.
        // Les variables unitPrice, totalAmount, fee sont restées à 0.
        // final_tx_type est resté UNKNOWN, ou mappé rapidement dans le bloc if (!session||!wallet).

        // On ré-assure le mapping du type pour la transaction FAILED préliminaire
         if (request.type == RequestType::BUY) final_tx_type = TransactionType::BUY;
         else if (request.type == RequestType::SELL) final_tx_type = TransactionType::SELL;
         else final_tx_type = TransactionType::UNKNOWN;


        // Construction de l'objet Transaction FAILED via make_shared
         final_transaction_ptr = std::make_shared<Transaction>(
            transactionId,      // ID généré plus tôt
            request.clientId,
            final_tx_type,      // Type mappé (correct maintenant même si UNKNOWN)
            request.cryptoName, // Nom de la crypto (peut être UNKNOWN)
            request.quantity,   // Quantité demandée
            unitPrice,          // Sera 0.0
            totalAmount,        // Sera 0.0
            fee,                // Sera 0.0
            timestamp_t,        // Timestamp généré plus tôt
            status,             // Sera FAILED (set dans le bloc if (!session || !wallet))
            failureReason       // Raison (set dans le bloc if (!session || !wallet))
        );
        LOG("TransactionQueue::processRequest INFO : Création Transaction finale FAILED (Session/Wallet null ou échec précoce) pour client " + request.clientId, "INFO");
    }

    // À ce point, final_transaction_ptr est GARANTI non-null (soit créé sous verrou, soit créé ici en FAILED).


    // --- Logguer la transaction finale globalement ---
    // On utilise ici l'objet via le shared_ptr.
    // Transaction::logTransactionToCSV doit être thread-safe.
    // TODO: Définir le chemin du fichier CSV global (assurez-vous que ../src/data existe).
    Transaction::logTransactionToCSV("../src/data/global_transactions.csv", *final_transaction_ptr); // Déréférencer le shared_ptr.
     LOG("TransactionQueue::processRequest INFO : Transaction ID: " + final_transaction_ptr->getId() + " logguée globalement pour client " + final_transaction_ptr->getClientId() + " avec statut: " + transactionStatusToString(final_transaction_ptr->getStatus()), "INFO");


    // --- Notifier la ClientSession correspondante ---
    // Utilisez le sessionPtr obtenu plus tôt.
    if (session) { // session est le shared_ptr obtenu de la map plus tôt (il pourrait être null ici si le cas initial était !session)
        try {
            // Si session est non-null, on appelle applyTransactionRequest.
            // Si session était null au début, ce bloc n'est pas exécuté, ce qui est correct.
            session->applyTransactionRequest(*final_transaction_ptr); // Appel de notification (déréférencement)

        } catch (const std::exception& e) {
            LOG("TransactionQueue::processRequest ERROR : Exception lors de l'appel à applyTransactionRequest pour client ID: " + request.clientId + ", Transaction ID: " + final_transaction_ptr->getId() + ". Erreur: " + std::string(e.what()), "ERROR");
        } catch (...) {
             LOG("TransactionQueue::processRequest ERROR : Exception inconnue lors de l'appel à applyTransactionRequest pour client ID: " + request.clientId + ", Transaction ID: " + final_transaction_ptr->getId() + ".", "ERROR");
        }
    } else {
         // Ce log s'affiche si session était null au début.
         LOG("TransactionQueue::processRequest WARNING : ClientSession introuvable ou invalide pour client ID: " + request.clientId + " lors de la notification. Transaction ID: " + final_transaction_ptr->getId() + ". Le résultat ne sera pas appliqué à la session.", "WARNING");
    }


    // Log de fin de la fonction
    LOG("TransactionQueue::processRequest INFO : --- Fin traitement requête ID: " + transactionId + " pour client " + request.clientId + " avec statut final: " + transactionStatusToString(final_transaction_ptr->getStatus()) + " ---", "INFO");

    // La variable shared_ptr<Transaction> final_transaction_ptr sort de portée ici et libère l'objet Transaction si c'était le dernier shared_ptr.

} // Fin du corps de la fonction processRequest


// --- Implémentation de addRequest ---
void TransactionQueue::addRequest(const TransactionRequest& request) { // Signature modifiée
    if (!running.load(std::memory_order_acquire)) {
        LOG("TransactionQueue::addRequest Erreur : Tentative d'ajouter une requête alors que la file n'est pas en cours d'exécution pour client ID: " + request.clientId + ", Type: " + requestTypeToString(request.type), "ERROR");
        return;
    }

    { // Section critique pour la file
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(request); // 'request' est une const ref, elle est copiée dans la queue
    } // Le verrou est libéré

    cv.notify_one();
}


/*
#include "../headers/TransactionQueue.h"
#include "../headers/ClientSession.h" // Pour accéder à ClientSession et son Wallet
#include "../headers/Logger.h" 
#include "../headers/Global.h" 
#include "../headers/Transaction.h"


// Includes pour les fonctionnalités standards :
#include <iostream>
#include <chrono>
#include <utility>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <string>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cctype>


// --- Implémentation de requestTypeToString ---
// Convertit une RequestType enum en string.
std::string requestTypeToString(RequestType type) {
    switch (type) {
        case RequestType::BUY: return "BUY";
        case RequestType::SELL: return "SELL";
        case RequestType::UNKNOWN_REQUEST: return "UNKNOWN_REQUEST";
        default: return "UNKNOWN_REQUEST"; // Devrait pas arriver avec les types définis
    }
}


// --- Initialisation des membres NON statiques ---
// Ils sont initialisés dans le constructeur.


// --- Constructeur de TransactionQueue ---
// Initialise le flag running à false.
TransactionQueue::TransactionQueue() : running(false) {
}

// --- Destructeur de TransactionQueue ---
// Assure que le thread worker est arrêté proprement.
TransactionQueue::~TransactionQueue() {
    stop(); // Appelle la méthode stop()
}

// --- Enregistre une ClientSession ---
// Stocke un weak_ptr vers la session dans sessionMap. Thread-safe.
void TransactionQueue::registerSession(const std::shared_ptr<ClientSession>& session) {
    if (!session) {
        LOG("TransactionQueue::registerSession Erreur : Tentative d'enregistrer une session null.", "ERROR");
        return;
    }
    std::lock_guard<std::mutex> lock(sessionMapMtx);
    sessionMap[session->getClientId()] = session; // session->getId() est thread-safe (const)
    LOG("TransactionQueue::registerSession Session enregistrée pour client ID: " + session->getClientId(), "INFO");
}

// --- Désenregistre une ClientSession ---
// Supprime l'entrée de sessionMap. Thread-safe.
void TransactionQueue::unregisterSession(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(sessionMapMtx);
    auto it = sessionMap.find(clientId);
    if (it != sessionMap.end()) {
        sessionMap.erase(it);
        LOG("TransactionQueue::unregisterSession Session désenregistrée pour client ID: " + clientId, "INFO");
    } else {
        LOG("TransactionQueue::unregisterSession Session pour client ID: " + clientId + " non trouvée dans sessionMap lors du désenregistrement.", "WARNING");
    }
}

// --- Implémentation de start ---
// Démarre le thread de traitement de la file. Appelée par le Server au démarrage.
void TransactionQueue::start() {
    if (!running.load(std::memory_order_acquire)) {
        running.store(true, std::memory_order_release);

        // Détermine la taille du pool de threads
        threadPoolSize = std::max(4, static_cast<int>(std::thread::hardware_concurrency()));

        // Crée les threads du pool
        for (int i = 0; i < threadPoolSize; ++i) {
            workers.emplace_back(&TransactionQueue::process, this);
        }

        LOG("TransactionQueue::start INFO : Pool de threads démarré avec " + std::to_string(threadPoolSize) + " threads.", "INFO");
    } else {
        LOG("TransactionQueue::start WARNING : TransactionQueue déjà en cours d'exécution.", "WARNING");
    }
}

// --- Demande l'arrêt du thread et attend sa fin ---
// Appelée par le Server ou le destructeur. Thread-safe.
void TransactionQueue::stop() {
    if (running.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            running.store(false, std::memory_order_release);
        }

        cv.notify_all(); // Réveille tous les threads

        // Joins tous les threads
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers.clear(); // Nettoie le pool de threads
        LOG("TransactionQueue::stop INFO : Pool de threads arrêté proprement.", "INFO");
    } else {
        LOG("TransactionQueue::stop WARNING : TransactionQueue déjà arrêté.", "WARNING");
    }
}

// --- Fonction principale du thread worker ---
// Tourne en boucle, attendant et traitant les requêtes.
void TransactionQueue::process() {
    LOG("TransactionQueue::process INFO : Thread de traitement démarré.", "INFO");

    while (true) {
        TransactionRequest req;

        {
            std::unique_lock<std::mutex> lock(mtx);

            // Attendre qu'il y ait une requête ou que l'arrêt soit demandé
            cv.wait(lock, [&] {
                return !queue.empty() || !running.load(std::memory_order_acquire);
            });

            // Condition de sortie : arrêt demandé et file vide
            if (!running.load(std::memory_order_acquire) && queue.empty()) {
                LOG("TransactionQueue::process INFO : Conditions d'arrêt atteintes. Fin du thread.", "INFO");
                break;
            }

            // Récupère une requête de la file
            if (!queue.empty()) {
                req = queue.front();
                queue.pop();
            }
        } // Libère le verrou

        // Traite la requête en dehors du verrou
        if (!req.clientId.empty()) {
            LOG("TransactionQueue::process INFO : Traitement de la requête pour client ID: " + req.clientId, "INFO");
            processRequest(req);
        }
    }
}


// --- Implémentation de processRequest ---
// Prend une requête par const référence, effectue le traitement sous verrou du Wallet,
// met à jour le Wallet, ajoute à l'historique, crée la Transaction finale,
// et notifie la ClientSession.
void TransactionQueue::processRequest(const TransactionRequest& request) {
    // Log d'entrée de la fonction (utile pour voir que la TQ prend la requête)
    LOG("TransactionQueue::processRequest INFO : Début traitement requête ID client: " + request.clientId + ", Type: " + requestTypeToString(request.type) + ", Quantité: " + std::to_string(request.quantity), "INFO");

    // Variables pour les résultats du traitement
    std::string transactionId = "";
    TransactionType final_tx_type = TransactionType::UNKNOWN;
    double unitPrice = 0.0;
    double totalAmount = 0.0;
    double fee = 0.0; // TODO: Calculer les frais réels si nécessaire
    TransactionStatus status = TransactionStatus::PENDING; // Commence à PENDING, statut final déterminé sous verrou
    std::string failureReason = "";

    // Déclaration du shared_ptr pour la transaction finale.
    // Sera nullptr si la transaction échoue avant la création de l'objet Transaction sous verrou.
    std::shared_ptr<Transaction> final_transaction_ptr = nullptr;


    // --- Génération ID et Timestamp ---
    transactionId = Transaction::generateNewIdString(); // Génère un ID unique pour cette requête/transaction
    auto timestamp = std::chrono::system_clock::now(); // Timestamp actuel
    std::time_t timestamp_t = std::chrono::system_clock::to_time_t(timestamp);


    // --- Préliminaires (accès session/wallet) ---
    std::shared_ptr<ClientSession> session = nullptr;
    std::shared_ptr<Wallet> wallet = nullptr;

    { // Verrou pour accéder à sessionMap
        std::lock_guard<std::mutex> sessionLock(sessionMapMtx);
        auto it = sessionMap.find(request.clientId);
        if (it != sessionMap.end()) {
            session = it->second.lock(); // Tente d'obtenir un shared_ptr
            if (session) {
                wallet = session->getClientWallet(); // getClientWallet() doit retourner shared_ptr<Wallet>
            }
        }
    } // Verrou sessionMap libéré


    // --- Vérification préliminaire Session/Wallet ---
    if (!session || !wallet) {
        status = TransactionStatus::FAILED;
        failureReason = "Client session or wallet not available.";
        LOG("TransactionQueue::processRequest ERROR : ClientSession ou Wallet introuvable pour client ID: " + request.clientId + ". Transaction FAILED préliminaire.", "ERROR");
        // Les variables unitPrice, totalAmount, fee restent à 0.0
        // final_tx_type reste UNKNOWN (ou mappé rapidement avant création Transaction FAILED).
        // L'objet Transaction FAILED sera créé APRÈS le grand 'else'.

    } else { // Session et Wallet disponibles : On peut procéder à la logique sous verrou

        // --- Obtention du prix (avant le bloc verrouillé principal) ---
        unitPrice = Global::getPrice("SRD-BTC"); // Assurez-vous que ce symbole est correct
        // Note : Le prix est re-vérifié sous verrou.


        // --- LOGIQUE DE TRAITEMENT CRUCIALE SOUS VERROU DU WALLET ---
        { // Bloc pour le lock_guard sur le Wallet
            std::lock_guard<std::mutex> walletLock(wallet->getMutex()); // VERROUILLAGE RÉUSSI


            // Re-vérifier la validité du prix SOUS LE VERROU
             if (unitPrice <= 0 || !std::isfinite(unitPrice)) {
                 status = TransactionStatus::FAILED;
                 failureReason = "Invalid market price at execution.";
                 LOG("TransactionQueue::processRequest ERROR : Sous verrou Wallet: Prix invalide. Statut FAILED.", "ERROR");
             } else { // Prix valide

                // Calculer les montants réels et vérifier les fonds SOUS LE VERROU
                double amount_to_use_from_balance = 0.0; // Montant dans la devise du solde utilisé
                double quantity_to_trade_crypto = request.quantity; // Quantité de crypto demandée

                if (request.type == RequestType::BUY) {
                    double usd_cost = quantity_to_trade_crypto * unitPrice;
                    fee = usd_cost * 0.0001; // Frais sur le montant USD
                    totalAmount = usd_cost + fee; // Coût total en USD (montant échangé + frais)
                    amount_to_use_from_balance = totalAmount; // On utilise ce totalAmount du solde USD

                    if (wallet->getBalance(Currency::USD) >= amount_to_use_from_balance) { // APPEL POTENTIELLEMENT CRITIQUE wallet->getBalance !
                        // Fonds USD suffisants : La transaction peut être COMPLETED
                        status = TransactionStatus::COMPLETED;
                        LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": BUY validé sous verrou. Fonds USD suffisants.", "INFO");
                    } else {
                        status = TransactionStatus::FAILED;
                        failureReason = "Insufficient USD funds.";
                        LOG("TransactionQueue::processRequest WARNING : Client " + request.clientId + ": Solde USD insuffisant (" + std::to_string(wallet->getBalance(Currency::USD)) + ") pour BUY de " + std::to_string(amount_to_use_from_balance) + " USD. Transaction FAILED.", "WARNING");
                    }

                } else if (request.type == RequestType::SELL) {
                    amount_to_use_from_balance = quantity_to_trade_crypto;
                    totalAmount = quantity_to_trade_crypto * unitPrice;
                    fee = totalAmount * 0.0001;
                    totalAmount -= fee;

                    if (wallet->getBalance(Currency::SRD_BTC) >= amount_to_use_from_balance) { // APPEL POTENTIELLEMENT CRITIQUE wallet->getBalance !
                        status = TransactionStatus::COMPLETED;
                        LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": SELL validé sous verrou. Fonds SRD-BTC suffisants.", "INFO");
                    } else {
                        status = TransactionStatus::FAILED;
                        failureReason = "Insufficient SRD-BTC funds.";
                        LOG("TransactionQueue::processRequest WARNING : Client " + request.clientId + ": Solde SRD-BTC insuffisant (" + std::to_string(wallet->getBalance(Currency::SRD_BTC)) + ") pour SELL de " + std::to_string(amount_to_use_from_balance) + " SRD-BTC. Transaction FAILED.", "WARNING");
                    }
                } else { // Type non supporté
                    status = TransactionStatus::FAILED;
                    failureReason = "Unsupported or unknown request type.";
                    LOG("TransactionQueue::processRequest ERROR : Sous verrou Wallet: Type de requête non supporté/inconnu.", "ERROR");
                }


                // --- Si la transaction est COMPLETED, mettre à jour les soldes ---
                if (status == TransactionStatus::COMPLETED) {
                    if (request.type == RequestType::BUY) {
                        wallet->updateBalance(Currency::USD, -totalAmount); // APPEL POTENTIELLEMENT CRITIQUE wallet->updateBalance !
                        wallet->updateBalance(Currency::SRD_BTC, request.quantity); // APPEL POTENTIELLEMENT CRITIQUE
                    } else if (request.type == RequestType::SELL) {
                         wallet->updateBalance(Currency::SRD_BTC, -request.quantity); // APPEL POTENTIELLEMENT CRITIQUE wallet->updateBalance !
                         wallet->updateBalance(Currency::USD, totalAmount); // APPEL POTENTIELLEMENT CRITIQUE
                    }

                    wallet->saveToFile(); // APPEL POTENTIELLEMENT CRITIQUE wallet->saveToFile !
                    LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": Wallet sauvegardé après Tx COMPLETED.", "INFO");

                } // Fin if (status == TransactionStatus::COMPLETED)

                // --- Créer l'objet Transaction final et l'ajouter à l'historique ---
                // Ces étapes se font ICI, sous le verrou du Wallet, après les mises à jour.
                // L'objet est créé avec le statut (COMPLETED/FAILED) déterminé ci-dessus.

                 // Mapper RequestType à TransactionType pour l'objet Transaction final.
                 if (request.type == RequestType::BUY) final_tx_type = TransactionType::BUY;
                 else if (request.type == RequestType::SELL) final_tx_type = TransactionType::SELL;
                 else final_tx_type = TransactionType::UNKNOWN; // Cas fallback si type inconnu


                // Construction de l'objet Transaction finale (via make_shared)
                // Les variables transactionId, timestamp_t, status, failureReason, etc.
                // ont toutes été définies à ce point si le grand else { Session/Wallet valides } a été exécuté.
                final_transaction_ptr = std::make_shared<Transaction>( // Construction via make_shared ici, sous verrou !
                    transactionId,      // Utilise l'ID généré plus tôt (avant grand if/else)
                    request.clientId,
                    final_tx_type,      // Type mappé
                    request.cryptoName, // Nom de la crypto demandée
                    request.quantity,   // Quantité demandée
                    unitPrice,          // Prix effectif d'exécution
                    totalAmount,        // Montant total échangé
                    fee,                // Frais
                    timestamp_t,        // Timestamp généré plus tôt
                    status,             // Le statut (COMPLETED/FAILED) déterminé ci-dessus !
                    failureReason       // Raison d'échec si FAILED
                );


                // Ajouter la transaction à l'historique du Wallet.
                wallet->addTransaction(*final_transaction_ptr); // Appel addTransaction SOUS VERROU (avec déréférencement) !
                LOG("TransactionQueue::processRequest INFO : Transaction " + final_transaction_ptr->getId() + " ajoutée à l'historique du Wallet sous verrou pour client " + request.clientId + ". Statut: " + transactionStatusToString(final_transaction_ptr->getStatus()), "INFO");


            } // Fin else (prix valide sous verrou)
            // === FIN DU CONTENU QUI DOIT ETRE DANS CE BLOC VERROUILLÉ ! ===

        } // Le lock_guard walletLock libère le mutex du Wallet ici !

    } // Fin du grand 'else' (Session et Wallet disponibles)


    // === Après le grand 'else' (Session et Wallet disponibles) ===
    // Si Session/Wallet n'étaient PAS disponibles, le bloc ci-dessus est sauté.
    // final_transaction_ptr est toujours nullptr dans ce cas. Il faut quand même créer une transaction FAILED
    // pour le logging global et la notification (si session existe).


    // --- Gérer la Transaction finale (Créer FAILED si nécessaire, puis Log & Notifier) ---
    // Si final_transaction_ptr est nullptr, cela signifie que le grand 'else' (où le Wallet est géré) n'a pas été exécuté,
    // donc la transaction a échoué très tôt (Session/Wallet manquants).
    // On crée ici l'objet Transaction FAILED pour le logging global et la notification si nécessaire.
    if (!final_transaction_ptr) {
        // Les variables de résultat (status, failureReason) ont été réglées dans le bloc if (!session || !wallet)
        // Les variables transactionId, timestamp, timestamp_t ont été générées en haut.
        // Les variables unitPrice, totalAmount, fee sont restées à 0.
        // final_tx_type est resté UNKNOWN, ou mappé rapidement dans le bloc if (!session||!wallet).

        // On ré-assure le mapping du type pour la transaction FAILED préliminaire
         if (request.type == RequestType::BUY) final_tx_type = TransactionType::BUY;
         else if (request.type == RequestType::SELL) final_tx_type = TransactionType::SELL;
         else final_tx_type = TransactionType::UNKNOWN;


        // Construction de l'objet Transaction FAILED via make_shared
         final_transaction_ptr = std::make_shared<Transaction>(
            transactionId,      // ID généré plus tôt
            request.clientId,
            final_tx_type,      // Type mappé (correct maintenant même si UNKNOWN)
            request.cryptoName, // Nom de la crypto (peut être UNKNOWN)
            request.quantity,   // Quantité demandée
            unitPrice,          // Sera 0.0
            totalAmount,        // Sera 0.0
            fee,                // Sera 0.0
            timestamp_t,        // Timestamp généré plus tôt
            status,             // Sera FAILED (set dans le bloc if (!session || !wallet))
            failureReason       // Raison (set dans le bloc if (!session || !wallet))
        );
        LOG("TransactionQueue::processRequest INFO : Création Transaction finale FAILED (Session/Wallet null ou échec précoce) pour client " + request.clientId, "INFO");
    }

    // À ce point, final_transaction_ptr est GARANTI non-null (soit créé sous verrou, soit créé ici en FAILED).


    // --- Logguer la transaction finale globalement ---
    // On utilise ici l'objet via le shared_ptr.
    // Transaction::logTransactionToCSV doit être thread-safe.
    // TODO: Définir le chemin du fichier CSV global (assurez-vous que ../src/data existe).
    Transaction::logTransactionToCSV("../src/data/global_transactions.csv", *final_transaction_ptr); // Déréférencer le shared_ptr.
     LOG("TransactionQueue::processRequest INFO : Transaction ID: " + final_transaction_ptr->getId() + " logguée globalement pour client " + final_transaction_ptr->getClientId() + " avec statut: " + transactionStatusToString(final_transaction_ptr->getStatus()), "INFO");


    // --- Notifier la ClientSession correspondante ---
    // Utilisez le sessionPtr obtenu plus tôt.
    if (session) { // session est le shared_ptr obtenu de la map plus tôt (il pourrait être null ici si le cas initial était !session)
        try {
            // Si session est non-null, on appelle applyTransactionRequest.
            // Si session était null au début, ce bloc n'est pas exécuté, ce qui est correct.
            session->applyTransactionRequest(*final_transaction_ptr); // Appel de notification (déréférencement)

        } catch (const std::exception& e) {
            LOG("TransactionQueue::processRequest ERROR : Exception lors de l'appel à applyTransactionRequest pour client ID: " + request.clientId + ", Transaction ID: " + final_transaction_ptr->getId() + ". Erreur: " + std::string(e.what()), "ERROR");
        } catch (...) {
             LOG("TransactionQueue::processRequest ERROR : Exception inconnue lors de l'appel à applyTransactionRequest pour client ID: " + request.clientId + ", Transaction ID: " + final_transaction_ptr->getId() + ".", "ERROR");
        }
    } else {
         // Ce log s'affiche si session était null au début.
         LOG("TransactionQueue::processRequest WARNING : ClientSession introuvable ou invalide pour client ID: " + request.clientId + " lors de la notification. Transaction ID: " + final_transaction_ptr->getId() + ". Le résultat ne sera pas appliqué à la session.", "WARNING");
    }


    // Log de fin de la fonction
    LOG("TransactionQueue::processRequest INFO : --- Fin traitement requête ID: " + transactionId + " pour client " + request.clientId + " avec statut final: " + transactionStatusToString(final_transaction_ptr->getStatus()) + " ---", "INFO");

    // La variable shared_ptr<Transaction> final_transaction_ptr sort de portée ici et libère l'objet Transaction si c'était le dernier shared_ptr.

} // Fin du corps de la fonction processRequest


// --- Implémentation de addRequest ---
void TransactionQueue::addRequest(const TransactionRequest& request) {
    if (!running.load(std::memory_order_acquire)) {
        LOG("TransactionQueue::addRequest ERROR : Tentative d'ajouter une requête alors que la queue est arrêtée.", "ERROR");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(request);
    }

    cv.notify_one(); // Notifie un thread disponible
    LOG("TransactionQueue::addRequest INFO : Requête ajoutée pour client ID: " + request.clientId, "INFO");
}
    */