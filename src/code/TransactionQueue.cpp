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

// --- Implémentation Corrigée Finale de processRequest ---
// Prend une requête par const référence, effectue le traitement sous verrou du Wallet,
// met à jour le Wallet, ajoute à l'historique, crée la Transaction finale,
// et notifie la ClientSession.
void TransactionQueue::processRequest(const TransactionRequest& request) {
    //LOG("TransactionQueue::processRequest Début traitement logique pour requête ID client: " + request.clientId + ", Type: " + requestTypeToString(request.type) + ", Quantité: " + std::to_string(request.quantity), "DEBUG");

    // Variables pour les résultats du traitement (celles qui sont déterminées pendant le traitement)
    std::string transactionId = ""; // Sera généré sous verrou
    TransactionType final_tx_type = TransactionType::UNKNOWN; // Sera mappé sous verrou
    double unitPrice = 0.0; // Sera obtenu avant ou sous verrou
    double totalAmount = 0.0; // Sera calculé sous verrou
    double fee = 0.0; // Sera calculé sous verrou
    TransactionStatus status = TransactionStatus::PENDING; // Commence à PENDING, statut final sous verrou
    std::string failureReason = ""; // Sera set sous verrou si échec
    auto timestamp = std::chrono::system_clock::now(); // Sera obtenu sous verrou
    std::time_t timestamp_t = std::chrono::system_clock::to_time_t(timestamp);


    // --- Préliminaires (accès session/wallet, prix) ---
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

    // === Déplacer l'obtention du prix ICI, avant le bloc verrouillé principal ===
    // Le prix est nécessaire pour le calcul des montants, qui se fait sous verrou.
    unitPrice = Global::getPrice("SRD-BTC"); // Assurez-vous que ce symbole est correct

    // --- Vérification préliminaire Session/Wallet ---
    if (!session || !wallet) {
        // La session ou le portefeuille n'est pas disponible (la session a dû se déconnecter entre temps)
        status = TransactionStatus::FAILED;
        failureReason = "Client session or wallet not available.";
        LOG("TransactionQueue::processRequest ClientSession ou Wallet introuvable pour client ID: " + request.clientId + ". Transaction FAILED.", "ERROR");
        // On ne peut pas créer la Transaction finale ici si le wallet est null,
        // MAIS on PEUT créer une Transaction FAILED plus tard avec les infos de la request.
        // La création et notification se feront après le bloc verrouillé (qui sera ignoré si wallet est null).
         unitPrice = 0.0; // S'assurer que unitPrice est à 0 ou invalide si prix non obtenu
         totalAmount = 0.0; // S'assurer que les montants sont à 0 si échec préliminaire
         fee = 0.0;
         // status et failureReason sont déjà réglés.

    } else { // Session et Wallet disponibles : On peut procéder à la logique sous verrou
        // --- LOGIQUE DE TRAITEMENT CRUCIALE SOUS VERROU DU WALLET ---
        { // Bloc pour le lock_guard sur le Wallet
            // Obtenir le mutex du Wallet et le verrouiller.
            // REQUIERT que Wallet ait une méthode publique std::mutex& getMutex();
            std::lock_guard<std::mutex> walletLock(wallet->getMutex()); // <-- VERROUILLAGE DU WALLET !

            // Re-vérifier la validité du prix SOUS LE VERROU (même si obtenu avant, sécurité)
             if (unitPrice <= 0 || !std::isfinite(unitPrice)) {
                 status = TransactionStatus::FAILED;
                 failureReason = "Invalid market price at execution.";
                 std::stringstream ss_log;
                 // Correction des caractères accentués
                 ss_log << "TransactionQueue::processRequest Prix invalide (" << std::fixed << std::setprecision(10) << unitPrice
                        << ") SOUS VERROU pour requête " << requestTypeToString(request.type)
                        << " client " << request.clientId << ". Transaction FAILED.";
                 LOG(ss_log.str(), "ERROR");
             } else {
                // Calculer les montants réels et vérifier les fonds SOUS LE VERROU
                double amount_to_use_from_balance = 0.0; // Montant dans la devise du solde utilisé
                double quantity_to_trade_crypto = request.quantity; // Quantité de crypto demandée

                if (request.type == RequestType::BUY) {
                    // Pour un BUY, la 'quantity' de la requête est la quantité de SRD-BTC voulue.
                    // On doit calculer le coût en USD et vérifier le solde USD.
                    double usd_cost = quantity_to_trade_crypto * unitPrice;
                    fee = usd_cost * 0.0001; // Frais sur le montant USD
                    totalAmount = usd_cost + fee; // Coût total en USD (montant échangé + frais)
                    amount_to_use_from_balance = totalAmount; // On utilise ce totalAmount du solde USD

                    if (wallet->getBalance(Currency::USD) >= amount_to_use_from_balance) {
                        status = TransactionStatus::COMPLETED; // Fonds suffisants
                        //LOG("TransactionQueue::processRequest Client " + request.clientId + ": BUY validé sous verrou. Fonds USD suffisants.", "INFO");
                    } else {
                        status = TransactionStatus::FAILED;
                        failureReason = "Insufficient USD funds.";
                        //LOG("TransactionQueue::processRequest Client " + request.clientId + ": Solde USD insuffisant (" + std::to_string(wallet->getBalance(Currency::USD)) + ") pour BUY de " + std::to_string(amount_to_use_from_balance) + " USD. Transaction FAILED.", "WARNING");
                    }

                } else if (request.type == RequestType::SELL) {
                     // Pour un SELL, la 'quantity' de la requête est la quantité de SRD-BTC à vendre.
                     // On doit vérifier le solde SRD-BTC. Le montant total reçu sera en USD.
                     amount_to_use_from_balance = quantity_to_trade_crypto; // On utilise cette quantité du solde SRD-BTC
                     totalAmount = quantity_to_trade_crypto * unitPrice; // Montant total reçu en USD (avant frais)
                     fee = totalAmount * 0.0001; // Frais sur le montant USD reçu
                     totalAmount -= fee; // Montant total NET reçu en USD

                     if (wallet->getBalance(Currency::SRD_BTC) >= amount_to_use_from_balance) {
                        status = TransactionStatus::COMPLETED; // Fonds suffisants
                         //LOG("TransactionQueue::processRequest Client " + request.clientId + ": SELL validé sous verrou. Fonds SRD-BTC suffisants.", "INFO");
                     } else {
                         status = TransactionStatus::FAILED;
                         failureReason = "Insufficient SRD-BTC funds.";
                         //LOG("TransactionQueue::processRequest Client " + request.clientId + ": Solde SRD-BTC insuffisant (" + std::to_string(wallet->getBalance(Currency::SRD_BTC)) + ") pour SELL de " + std::to_string(amount_to_use_from_balance) + " SRD-BTC. Transaction FAILED.", "WARNING");
                     }
                } else {
                    // Type de requête inconnu ou non géré à ce stade (ne devrait pas arriver si parsing correct)
                     status = TransactionStatus::FAILED;
                     failureReason = "Unsupported or unknown request type.";
                     // Correction des caractères accentués
                     //LOG("TransactionQueue::processRequest Requête avec type non supporté/inconnu SOUS VERROU pour client ID: " + request.clientId + ", Type: " + requestTypeToString(request.type) + ". Transaction FAILED.", "ERROR");
                }

                // --- Si la transaction est COMPLETED, mettre à jour les soldes ---
                if (status == TransactionStatus::COMPLETED) {
                    if (request.type == RequestType::BUY) {
                        // Débiter les USD dépensés (totalAmount INCLUT les frais)
                        wallet->updateBalance(Currency::USD, -totalAmount); // <-- Appel updateBalance SOUS VERROU
                        // Créditer les SRD-BTC reçus (quantity est la quantité achetée)
                        wallet->updateBalance(Currency::SRD_BTC, request.quantity); // <-- Appel updateBalance SOUS VERROU
                        // Correction des caractères accentués
                        LOG("TransactionQueue::processRequest Client " + request.clientId + ": Soldes Wallet mis à jour pour BUY COMPLETED. Débit " + std::to_string(totalAmount) + " USD, Crédit " + std::to_string(request.quantity) + " SRD-BTC.", "DEBUG");

                    } else if (request.type == RequestType::SELL) {
                         // Débiter les SRD-BTC vendus (quantity est la quantité vendue)
                         wallet->updateBalance(Currency::SRD_BTC, -request.quantity); // <-- Appel updateBalance SOUS VERROU
                         // Créditer les USD reçus (totalAmount est le montant NET reçu après frais)
                         wallet->updateBalance(Currency::USD, totalAmount); // <-- Appel updateBalance SOUS VERROU
                         // Correction des caractères accentués
                         LOG("TransactionQueue::processRequest Client " + request.clientId + ": Soldes Wallet mis à jour pour SELL COMPLETED. Débit " + std::to_string(request.quantity) + " SRD-BTC, Crédit " + std::to_string(totalAmount) + " USD.", "DEBUG");
                    }

                    // Sauvegarder le Wallet après la mise à jour des soldes (idéalement atomique, mais saveToFile simple est OK pour benchmark)
                    wallet->saveToFile(); // <-- Appel saveToFile SOUS VERROU
                    // Correction des caractères accentués
                    LOG("TransactionQueue::processRequest Client " + request.clientId + ": Wallet sauvegardé après Tx COMPLETED.", "DEBUG");

                } // Fin if (status == TransactionStatus::COMPLETED)

                // --- Créer l'objet Transaction final et l'ajouter à l'historique ---
                // Ces étapes se font MAINTENANT sous le verrou du Wallet !
                // L'ID et le timestamp sont générés ici.
                transactionId = Transaction::generateNewIdString(); // Génère un ID unique
                timestamp = std::chrono::system_clock::now(); // Timestamp actuel
                timestamp_t = std::chrono::system_clock::to_time_t(timestamp);

                // Mapper RequestType à TransactionType pour l'objet Transaction final.
                if (request.type == RequestType::BUY) final_tx_type = TransactionType::BUY;
                else if (request.type == RequestType::SELL) final_tx_type = TransactionType::SELL;
                // Si c'était UNKNOWN_REQUEST, final_tx_type reste UNKNOWN.

                // Construire l'objet Transaction finale avec TOUS les détails
                // Déclaration et construction se font ici, APRES tous les calculs et mises à jour de soldes sous verrou.
                // L'objet final_transaction sera visible dans le reste de la fonction après ce bloc verrouillé.
                Transaction final_transaction_local( // <-- Déclaration ET Construction ici ! Nom local temporaire.
                    transactionId,
                    request.clientId, // ID du client depuis la requête originale
                    final_tx_type,
                    request.cryptoName, // Nom de la crypto depuis la requête originale
                    request.quantity,   // Quantité demandée (ou exécutée si c'est la même)
                    unitPrice,          // Prix effectif d'exécution
                    totalAmount,        // Montant total échangé (ajusté des frais si applicable)
                    fee,                // Frais
                    timestamp_t,        // Timestamp de l'exécution
                    status,             // Statut final (COMPLETED ou FAILED)
                    failureReason       // Raison de l'échec
                );

                // Ajouter la transaction à l'historique du Wallet.
                wallet->addTransaction(final_transaction_local); // <-- Appel addTransaction SOUS VERROU !
                // Correction des caractères accentués
                LOG("TransactionQueue::processRequest Client " + request.clientId + ": Transaction " + final_transaction_local.getId() + " ajoutée à l'historique du Wallet sous verrou. Statut: " + transactionStatusToString(final_transaction_local.getStatus()), "INFO");

                // Après ce bloc verrouillé, l'objet final_transaction_local n'existe plus.
                // Pour le passer au logging et à la notification APRES le verrou, il faut le copier ou le rendre accessible.
                // Le plus simple est de copier les infos nécessaires ou passer une référence/pointeur créé DANS le bloc verrouillé.
                // Ou, plus propre, déclarer final_transaction AVANT le verrou, la CONSTRUIRE DANS le verrou,
                // et utiliser cette variable déclarée en dehors après le verrou.

                // === Reprenons la version avec déclaration en haut, mais construction DANS le verrou ===
                // C'est plus clair pour la visibilité après le verrou.
                // MAIS cela implique que Transaction ait une affectation valide, ce qui est le cas par défaut.
                // La variable déclarée en haut était: Transaction final_transaction;
                // La construction déplacée sera: final_transaction = Transaction(...);


            } // Fin else (prix valide)
        } // Le lock_guard walletLock libère le mutex du Wallet ici !

         // Si Session/Wallet étaient non disponibles, le bloc verrouillé ci-dessus est sauté.
         // Mais on a toujours besoin de créer l'objet Transaction (FAILED dans ce cas)
         // pour le logging global et la notification (si session existe).
         // La création de l'objet Transaction ne peut DONC PAS être entièrement DANS le else { Wallet disponible }.
         // Il faut la faire APRÈS le bloc verrouillé, mais avant logging/notification.
         // Les informations (status, failureReason, price, amounts, etc.) sont prêtes à ce point.

    } // Fin du grand 'else' (Session et Wallet disponibles)


    // --- Créer l'objet Transaction final ---
    // Cette section est PLACÉE ICI, APRÈS le bloc verrouillé (s'il a été exécuté),
    // mais AVANT logging/notification.
    // Les variables de résultat (transactionId, final_tx_type, unitPrice, etc.)
    // sont définies soit dans le bloc verrouillé (si Session/Wallet dispo),
    // soit avant (si Session/Wallet non dispo).
    // L'ID et le timestamp sont générés ici, car transactionId n'est assigné que si Wallet dispo actuellement.
    // C'est un peu délicat. Si Wallet non dispo, on ne génère pas l'ID ?
    // L'ID DOIT être généré pour chaque Transaction, même FAILED préliminaire.
    // La génération de l'ID et du timestamp DOIT se faire AVANT la création de l'objet Transaction.
    // Déplaçons la génération ID/timestamp PLUS HAUT, AVANT les vérifications préliminaires Session/Wallet.
    // Cela garantit qu'ID/timestamp sont TOUJOURS générés pour chaque requête traitée.

    // --- Déplacer la G\é{}n\é{}ration ID et Timestamp ICI (AVANT les vérifications préliminaires) ---
    transactionId = Transaction::generateNewIdString(); // Génère un ID unique pour chaque requête
    timestamp = std::chrono::system_clock::now(); // Timestamp actuel pour chaque requête
    timestamp_t = std::chrono::system_clock::to_time_t(timestamp);
    // Les variables transactionId, timestamp, timestamp_t sont maintenant définies ici.


    // --- Créer l'objet Transaction final ---
    // Cette section est PLACÉE ICI, APRÈS le bloc verrouillé (s'il a été exécuté),
    // mais AVANT logging/notification.
    // Les variables de résultat (status, failureReason, unitPrice, totalAmount, fee)
    // sont définies soit dans le bloc verrouillé (si Session/Wallet dispo et prix/fonds ok),
    // soit dans le bloc préliminaire (si Session/Wallet non dispo ou prix invalide ou fonds insuffisants dans le bloc verrouillé).

    // Mapper RequestType à TransactionType pour l'objet Transaction final.
    // Cette partie peut rester ici, car request.type est toujours valide si on arrive là.
    if (request.type == RequestType::BUY) final_tx_type = TransactionType::BUY;
    else if (request.type == RequestType::SELL) final_tx_type = TransactionType::SELL;
    // Si c'était UNKNOWN_REQUEST, final_tx_type reste UNKNOWN.


    // OK, on a toutes les infos (ID, timestamp, statut, raison, prix, montants, type)
    // Créons l'objet Transaction finale ICI.
    Transaction final_transaction_actual( // <-- Déclaration et Construction finale ici !
        transactionId,      // Utilise l'ID généré plus tôt
        request.clientId,   // ID du client depuis la requête originale
        final_tx_type,      // Type mappé
        request.cryptoName, // Nom de la crypto depuis la requête originale
        request.quantity,   // Quantité demandée (ou exécutée si c'est la même)
        unitPrice,          // Prix effectif d'exécution (0 si échec préliminaire/prix invalide)
        totalAmount,        // Montant total échangé (0 si échec)
        fee,                // Frais (0 si échec)
        timestamp_t,        // Timestamp généré plus tôt
        status,             // Statut final (PENDING -> COMPLETED/FAILED)
        failureReason       // Raison de l'échec (vide si succès)
    );

    // === L'appel à addTransaction DOIT se faire sous le verrou ! ===
    // Il n'est donc PAS ICI. Il est dans le bloc verrouillé si Wallet est disponible.
    // Si Wallet n'était PAS disponible, l'ajout à l'historique NE PEUT PAS se faire.

    // --- Logguer la transaction finale globalement ---
    // Transaction::logTransactionToCSV doit être thread-safe.
    // TODO: Définir le chemin du fichier CSV global.
    // On utilise ici l'objet fraîchement construit final_transaction_actual.
    Transaction::logTransactionToCSV("../src/data/global_transactions.csv", final_transaction_actual);

    // Correction des caractères accentués
    LOG("TransactionQueue::processRequest Transaction ID: " + final_transaction_actual.getId() + " logguée globalement avec statut: " + transactionStatusToString(final_transaction_actual.getStatus()) + " pour client " + final_transaction_actual.getClientId(), "INFO");


    // --- Notifier la ClientSession correspondante ---
    // Utilisez le sessionPtr obtenu plus tôt.
    // On utilise ici l'objet fraîchement construit final_transaction_actual.
    if (session) { // session est le shared_ptr obtenu de la map plus tôt
        try {
            // Appeler applyTransactionRequest avec l'objet Transaction final.
            // REQUIERT que ClientSession::applyTransactionRequest prenne const Transaction& ou const std::shared_ptr<Transaction>&
            session->applyTransactionRequest(final_transaction_actual); // <-- Appel de notification

            // Correction des caractères accentués
            LOG("TransactionQueue::processRequest Client " + final_transaction_actual.getClientId() + ": applyTransactionRequest appelé pour transaction ID: " + final_transaction_actual.getId() + ", Statut: " + transactionStatusToString(final_transaction_actual.getStatus()), "DEBUG");
        } catch (const std::exception& e) {
            // Correction des caractères accentués
            LOG("TransactionQueue::processRequest Exception lors de l'appel à applyTransactionRequest pour client ID: " + request.clientId + ", Transaction ID: " + final_transaction_actual.getId() + ". Erreur: " + std::string(e.what()), "ERROR");
        }
    } else {
        // Correction des caractères accentués
        LOG("TransactionQueue::processRequest ClientSession introuvable ou invalide pour client ID: " + request.clientId + " lors de la notification. Transaction ID: " + final_transaction_actual.getId() + ". Le résultat ne sera pas appliqué à la session.", "WARNING");
    }

    // Correction des caractères accentués
    LOG("TransactionQueue::processRequest Fin traitement logique pour requête ID client: " + request.clientId + ", Transaction ID: " + transactionId + ", Statut: " + transactionStatusToString(status), "DEBUG"); // Utiliser transactionId et status car final_transaction_actual scope local ? Non, elle est visible ici. Utilisons final_transaction_actual.getId() et getStatus().
     LOG("TransactionQueue::processRequest Fin traitement logique pour requête ID client: " + request.clientId + ", Transaction ID: " + final_transaction_actual.getId() + ", Statut: " + transactionStatusToString(final_transaction_actual.getStatus()), "DEBUG");
}


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