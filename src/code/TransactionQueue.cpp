// Implémentation de la classe TransactionQueue

#include "../headers/TransactionQueue.h"
#include "../headers/ClientSession.h" // Pour accéder à ClientSession et son Wallet
#include "../headers/Logger.h" // Macro LOG
#include "../headers/Global.h" // Global::getPrice
#include "../headers/Transaction.h" // Transaction struct, generateNewIdString, logTransactionToCSV, enums/helpers stringTo/ToString


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


// --- Implémentation de requestTypeToString (qui était un stub dans .h) ---
// Convertit une RequestType enum en string.
std::string requestTypeToString(RequestType type) {
    switch (type) {
        case RequestType::BUY: return "BUY";
        case RequestType::SELL: return "SELL";
        case RequestType::UNKNOWN_REQUEST: return "UNKNOWN_REQUEST";
        default: return "UNKNOWN_REQUEST"; // Devrait pas arriver avec les types définis
    }
}
// NOTE : Les implémentations des autres helpers (currencyToString, etc.) doivent se trouver dans Transaction.cpp


// --- Initialisation des membres NON statiques ---
// Ils sont initialisés dans le constructeur.


// --- Constructeur de TransactionQueue ---
// Initialise le flag running à false.
TransactionQueue::TransactionQueue() : running(false) {
    LOG("TransactionQueue créée.", "DEBUG");
}

// --- Destructeur de TransactionQueue ---
// Assure que le thread worker est arrêté proprement.
TransactionQueue::~TransactionQueue() {
    LOG("Destructeur TransactionQueue appelé. Arrêt du thread de traitement...", "DEBUG");
    stop(); // Appelle la méthode stop()
    LOG("Destructeur TransactionQueue terminé.", "DEBUG");
}

// --- Enregistre une ClientSession ---
// Stocke un weak_ptr vers la session dans sessionMap. Thread-safe.
void TransactionQueue::registerSession(const std::shared_ptr<ClientSession>& session) {
    if (!session) {
        LOG("TransactionQueue::registerSession Erreur : Tentative d'enregistrer une session null.", "ERROR");
        return;
    }
    std::lock_guard<std::mutex> lock(sessionMapMtx);
    sessionMap[session->getId()] = session; // session->getId() est thread-safe (const)
    LOG("TransactionQueue::registerSession Session enregistrée pour client ID: " + session->getId(), "INFO");
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
    // Vérifie si le thread n'est pas déjà en cours et s'il n'est pas joignable (état initial ou après un join)
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

//// Dans src/code/TransactionQueue.cpp

// ... (vos includes, autres fonctions TQ, y compris start() et stop()) ...

// --- Implémentation Finale Nettoyée de processRequest ---
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

    // *** DÉCLARATION DU SHARED_PTR POUR LA TRANSACTION FINALE ***
    // Sera nullptr si la transaction échoue avant la création de l'objet Transaction sous verrou.
    std::shared_ptr<Transaction> final_transaction_ptr = nullptr;
    // ********************************************************


    // --- Génération ID et Timestamp (pour toutes les requêtes traitées par la TQ) ---
    transactionId = Transaction::generateNewIdString(); // Génère un ID unique pour cette requête/transaction
    auto timestamp = std::chrono::system_clock::now(); // Timestamp actuel
    std::time_t timestamp_t = std::chrono::system_clock::to_time_t(timestamp);
    LOG("TransactionQueue::processRequest DEBUG : ID (" + transactionId + ") et Timestamp générés pour requête client " + request.clientId, "DEBUG");


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
        // Note : Le prix est re-vérifié sous verrou pour plus de sécurité et pour les calculs finaux.
        LOG("TransactionQueue::processRequest DEBUG : Prix SRD-BTC obtenu: " + std::to_string(unitPrice) + " pour client " + request.clientId, "DEBUG");


        // --- LOGIQUE DE TRAITEMENT CRUCIALE SOUS VERROU DU WALLET ---
        { // Bloc pour le lock_guard sur le Wallet
            std::lock_guard<std::mutex> walletLock(wallet->getMutex()); // <-- VERROUILLAGE RÉUSSI D'APRÈS LES LOGS
            LOG("TransactionQueue::processRequest DEBUG : Verrou Wallet obtenu pour client " + request.clientId, "DEBUG"); // <-- CE LOG S'AFFICHE

            // *** NOUVEAUX LOGS À L'INTÉRIEUR DU BLOC VERROUILLÉ ***

            LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Vérification validité prix " + std::to_string(unitPrice), "DEBUG"); // <-- Log AVANT vérif prix
            // Re-vérifier la validité du prix SOUS LE VERROU (et l'utiliser pour les calculs finaux)
             if (unitPrice <= 0 || !std::isfinite(unitPrice)) {
                 status = TransactionStatus::FAILED;
                 failureReason = "Invalid market price at execution.";
                 LOG("TransactionQueue::processRequest ERROR : Sous verrou Wallet: Prix invalide. Statut FAILED.", "ERROR"); // <-- Log prix invalide
             } else { // Prix valide
                LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Prix valide. Avant calculs et vérif fonds.", "DEBUG"); // <-- Log AVANT calculs/vérif

                // Calculer les montants réels et vérifier les fonds SOUS LE VERROU
                double amount_to_use_from_balance = 0.0; // Montant dans la devise du solde utilisé
                double quantity_to_trade_crypto = request.quantity; // Quantité de crypto demandée

                if (request.type == RequestType::BUY) {
                    // Logique pour un ordre BUY
                    LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Traitement BUY. Avant calculs montants.", "DEBUG"); // <-- Log avant calculs BUY
                    double usd_cost = quantity_to_trade_crypto * unitPrice;
                    fee = usd_cost * 0.0001; // Frais sur le montant USD
                    totalAmount = usd_cost + fee; // Coût total en USD (montant échangé + frais)
                    amount_to_use_from_balance = totalAmount; // On utilise ce totalAmount du solde USD
                    LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Après calculs BUY. Avant vérif solde USD.", "DEBUG"); // <-- Log avant vérif fonds BUY

                    if (wallet->getBalance(Currency::USD) >= amount_to_use_from_balance) { // <-- APPEL POTENTIELLEMENT CRITIQUE wallet->getBalance !
                        // Fonds USD suffisants : La transaction peut être COMPLETED
                        status = TransactionStatus::COMPLETED;
                        LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": BUY validé sous verrou. Fonds USD suffisants.", "INFO"); // <-- Log Succès BUY
                    } else {
                        status = TransactionStatus::FAILED;
                        failureReason = "Insufficient USD funds.";
                        LOG("TransactionQueue::processRequest WARNING : Client " + request.clientId + ": Solde USD insuffisant (" + std::to_string(wallet->getBalance(Currency::USD)) + ") pour BUY de " + std::to_string(amount_to_use_from_balance) + " USD. Transaction FAILED.", "WARNING"); // <-- Log Échec BUY
                    }

                } else if (request.type == RequestType::SELL) {
                    LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Traitement SELL. Avant calculs montants.", "DEBUG"); // <-- Log avant calculs SELL
                    amount_to_use_from_balance = quantity_to_trade_crypto;
                    totalAmount = quantity_to_trade_crypto * unitPrice;
                    fee = totalAmount * 0.0001;
                    totalAmount -= fee;
                    LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Après calculs SELL. Avant vérif solde SRD-BTC.", "DEBUG"); // <-- Log avant vérif fonds SELL

                    if (wallet->getBalance(Currency::SRD_BTC) >= amount_to_use_from_balance) { // <-- APPEL POTENTIELLEMENT CRITIQUE wallet->getBalance !
                        status = TransactionStatus::COMPLETED;
                        LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": SELL validé sous verrou. Fonds SRD-BTC suffisants.", "INFO"); // <-- Log Succès SELL
                    } else {
                        status = TransactionStatus::FAILED;
                        failureReason = "Insufficient SRD-BTC funds.";
                        LOG("TransactionQueue::processRequest WARNING : Client " + request.clientId + ": Solde SRD-BTC insuffisant (" + std::to_string(wallet->getBalance(Currency::SRD_BTC)) + ") pour SELL de " + std::to_string(amount_to_use_from_balance) + " SRD-BTC. Transaction FAILED.", "WARNING"); // <-- Log Échec SELL
                    }
                } else { // Type non supporté
                    status = TransactionStatus::FAILED;
                    failureReason = "Unsupported or unknown request type.";
                    LOG("TransactionQueue::processRequest ERROR : Sous verrou Wallet: Type de requête non supporté/inconnu.", "ERROR"); // <-- Log type non supporté
                }

                // Si le plantage n'est pas dans les vérifs/calculs, il est ICI ou après.
                LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Après vérifs fonds/calculs. Statut actuel: " + transactionStatusToString(status), "DEBUG"); // <-- Log après vérifs/calculs

                // --- Si la transaction est COMPLETED, mettre à jour les soldes ---
                if (status == TransactionStatus::COMPLETED) {
                    LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Statut COMPLETED. Avant mise à jour soldes.", "DEBUG"); // <-- Log avant updates
                    if (request.type == RequestType::BUY) {
                        wallet->updateBalance(Currency::USD, -totalAmount); // <-- APPEL POTENTIELLEMENT CRITIQUE wallet->updateBalance !
                        wallet->updateBalance(Currency::SRD_BTC, request.quantity); // <-- APPEL POTENTIELLEMENT CRITIQUE
                        LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Soldes mis à jour BUY.", "DEBUG"); // <-- Log après updates BUY
                    } else if (request.type == RequestType::SELL) {
                         wallet->updateBalance(Currency::SRD_BTC, -request.quantity); // <-- APPEL POTENTIELLEMENT CRITIQUE wallet->updateBalance !
                         wallet->updateBalance(Currency::USD, totalAmount); // <-- APPEL POTENTIELLEMENT CRITIQUE
                         LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Soldes mis à jour SELL.", "DEBUG"); // <-- Log après updates SELL
                    }

                    LOG("TransactionQueue::processRequest DEBUG : Sous verrou Wallet: Avant sauvegarde Wallet.", "DEBUG"); // <-- Log avant save
                    wallet->saveToFile(); // <-- APPEL POTENTIELLEMENT CRITIQUE wallet->saveToFile !
                    LOG("TransactionQueue::processRequest INFO : Client " + request.clientId + ": Wallet sauvegardé après Tx COMPLETED.", "INFO"); // <-- Log Wallet sauvegardé

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
                final_transaction_ptr = std::make_shared<Transaction>( // <-- Construction via make_shared ici, sous verrou !
                    transactionId,      // Utilise l'ID généré plus tôt (avant grand if/else)
                    request.clientId,
                    final_tx_type,      // Type mappé
                    request.cryptoName, // Nom de la crypto demandée
                    request.quantity,   // Quantité demandée
                    unitPrice,          // Prix effectif d'exécution
                    totalAmount,        // Montant total échangé
                    fee,                // Frais
                    timestamp_t,        // Timestamp généré plus tôt
                    status,             // <-- Le statut (COMPLETED/FAILED) déterminé ci-dessus !
                    failureReason       // Raison d'échec si FAILED
                );
                LOG("TransactionQueue::processRequest DEBUG : Transaction finale (via shared_ptr) construite SOUS VERROU pour client " + request.clientId, "DEBUG");


                // Ajouter la transaction à l'historique du Wallet.
                wallet->addTransaction(*final_transaction_ptr); // <-- Appel addTransaction SOUS VERROU (avec déréférencement) !
                LOG("TransactionQueue::processRequest INFO : Transaction " + final_transaction_ptr->getId() + " ajoutée à l'historique du Wallet sous verrou pour client " + request.clientId + ". Statut: " + transactionStatusToString(final_transaction_ptr->getStatus()), "INFO");


            } // Fin else (prix valide sous verrou)
            // === FIN DU CONTENU QUI DOIT ETRE DANS CE BLOC VERROUILLÉ ! ===

        } // Le lock_guard walletLock libère le mutex du Wallet ici !
        LOG("TransactionQueue::processRequest DEBUG : Verrou Wallet libéré pour client " + request.clientId, "DEBUG");

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
            session->applyTransactionRequest(*final_transaction_ptr); // <-- Appel de notification (déréférencement)

            LOG("TransactionQueue::processRequest DEBUG : applyTransactionRequest appelé pour client " + request.clientId + ", Transaction ID: " + final_transaction_ptr->getId() + ", Statut: " + transactionStatusToString(final_transaction_ptr->getStatus()), "DEBUG");
        } catch (const std::exception& e) {
            LOG("TransactionQueue::processRequest ERROR : Exception lors de l'appel à applyTransactionRequest pour client ID: " + request.clientId + ", Transaction ID: " + final_transaction_ptr->getId() + ". Erreur: " + std::string(e.what()), "ERROR");
        } catch (...) {
             LOG("TransactionQueue::processRequest ERROR : Exception inconnue lors de l'appel à applyTransactionRequest pour client ID: " + request.clientId + ", Transaction ID: " + final_transaction_ptr->getId() + ".", "ERROR");
        }
    } else {
         // Ce log s'affiche si session était null au début.
         LOG("TransactionQueue::processRequest WARNING : ClientSession introuvable ou invalide pour client ID: " + request.clientId + " lors de la notification. Transaction ID: " + final_transaction_ptr->getId() + ". Le résultat ne sera pas appliqué à la session.", "WARNING");
    }
    LOG("TransactionQueue::processRequest DEBUG : Après notification ClientSession pour client " + request.clientId, "DEBUG");


    // Log de fin de la fonction
    LOG("TransactionQueue::processRequest INFO : --- Fin traitement requête ID: " + transactionId + " pour client " + request.clientId + " avec statut final: " + transactionStatusToString(final_transaction_ptr->getStatus()) + " ---", "INFO");

    // La variable shared_ptr<Transaction> final_transaction_ptr sort de portée ici et libère l'objet Transaction si c'était le dernier shared_ptr.

} // Fin du corps de la fonction processRequest


// --- Implémentation de addRequest (Corrigée pour prendre const Request&) ---
void TransactionQueue::addRequest(const TransactionRequest& request) { // <-- Signature modifiée
    if (!running.load(std::memory_order_acquire)) {
        LOG("TransactionQueue::addRequest Erreur : Tentative d'ajouter une requête alors que la file n'est pas en cours d'exécution pour client ID: " + request.clientId + ", Type: " + requestTypeToString(request.type), "ERROR");
        return;
    }

    { // Section critique pour la file
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(request); // 'request' est une const ref, elle est copiée dans la queue
        std::stringstream ss_log;
        ss_log << "TransactionQueue::addRequest Requête ajoutée pour client ID: " << request.clientId
               << ", Type: " << requestTypeToString(request.type)
               << ", Quantité: " << std::fixed << std::setprecision(10) << request.quantity
               << ". Taille de la queue: " << queue.size();
        LOG(ss_log.str(), "DEBUG");
    } // Le verrou est libéré

    cv.notify_one();
}