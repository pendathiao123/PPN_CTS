#include "../headers/Bot.h"
#include "../headers/Global.h"      
#include "../headers/Logger.h"      
#include "../headers/Transaction.h" 
#include "../headers/Wallet.h"      
#include "../headers/ClientSession.h" 


#include <iostream>
#include <cmath>        
#include <numeric>      
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <mutex>        
#include <sstream>      
#include <iomanip>      
#include <chrono>
#include <thread>       
#include <future>
#include <stdexcept>    


// --- Implémentation des helpers de calcul (statistiques) ---

// Calcule la moyenne mobile simple (SMA).
double Bot::calculateSMA(const std::vector<double>& data) {
    if (data.empty()) {
        return 0.0;
    }
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / static_cast<double>(data.size());
}

// Calcule l'écart-type (version population).
double Bot::calculateStdDev(const std::vector<double>& data, double sma) {
    if (data.size() <= 1) {
        return 0.0;
    }
    double variance_sum = 0.0;
    for (double price : data) {
        variance_sum += std::pow(price - sma, 2);
    }
    return std::sqrt(variance_sum / static_cast<double>(data.size())); // Population standard deviation
}

// Calcule les Bandes de Bollinger en utilisant l'historique interne.
// Assume que botMutex est tenu par l'appelant (processLatestPrice).
BollingerBands Bot::calculateBands() const {
    // Pas de lock_guard ici car l'appelant (processLatestPrice) verrouille botMutex.

    size_t required_size = static_cast<size_t>(bollingerPeriod);
    if (priceHistoryForCalculation.size() < required_size) {
        // Pas assez de données.
        return {0.0, 0.0, 0.0};
    }

    // S'assurer qu'on prend les 'bollingerPeriod' derniers prix
    size_t start_index = priceHistoryForCalculation.size() - required_size;
    std::vector<double> relevant_prices(
        priceHistoryForCalculation.begin() + start_index,
        priceHistoryForCalculation.end()
    );

    double sma = calculateSMA(relevant_prices);
    double stddev = calculateStdDev(relevant_prices, sma);

    return {
        sma,                       // Middle Band
        sma + bollingerK * stddev, // Upper Band
        sma - bollingerK * stddev  // Lower Band
    };
}


// --- Constructeur ---
Bot::Bot(const std::string& id, int period, double k,
         std::shared_ptr<Wallet> wallet,
         std::weak_ptr<ClientSession> session_ptr)
    // Initialise les membres
    : clientId(id),
      bollingerPeriod(period),
      bollingerK(k),
      currentState(PositionState::NONE), // Commence sans position
      entryPrice(0.0),
      clientWallet(wallet),          // Stockage du shared_ptr Wallet
      clientSessionPtr(session_ptr), // Stockage du weak_ptr Session
      running(false)                 // Le bot ne tourne pas encore au moment de la construction
{
    // Validation basique des paramètres Bollinger
    if (this->bollingerPeriod <= 0) {
         LOG("Bot " + clientId + " - Avertissement : Période Bollinger invalide (" + std::to_string(this->bollingerPeriod) + "). Utilisation par défaut 20.", "WARNING");
         this->bollingerPeriod = 20;
    }
     if (this->bollingerK <= 0 || !std::isfinite(this->bollingerK)) {
         std::stringstream ss_log;
         ss_log << "Bot " << clientId << " - Avertissement : Facteur K Bollinger invalide (" << std::fixed << std::setprecision(2) << this->bollingerK << "). Utilisation par défaut 2.0.";
         LOG(ss_log.str(), "WARNING");
         this->bollingerK = 2.0;
     }

    LOG("Bot créé pour client ID : " + clientId +
        " avec période Bollinger = " + std::to_string(this->bollingerPeriod) +
        " et K = " + std::to_string(this->bollingerK), "INFO");
}

// --- Destructeur ---
Bot::~Bot() {
    // Assurez-vous que le thread est arrêté avant la destruction
    stop(); // Appelle stop() pour garantir l'arrêt propre du thread

    LOG("Bot détruit pour client ID : " + clientId, "INFO");
}


// --- Implémentation start() ---
void Bot::start() {
    bool expected = false;
    // Utilise compare_exchange_strong pour s'assurer que running était false avant de le mettre à true
    if (running.compare_exchange_strong(expected, true)) {
        // Démarre le thread si running était bien false
        LOG("Bot " + clientId + " - Démarrage du thread du bot...", "INFO");
        // Crée le thread et lui fait exécuter la méthode tradeLoop de cet objet Bot
        // std::thread prend un callable et ses arguments. Ici, le callable est la méthode membre &Bot::tradeLoop,
        // et le premier argument est l'instance sur laquelle l'appeler (this).
        botThread = std::thread(&Bot::tradeLoop, this);
        LOG("Bot " + clientId + " - Thread du bot lancé.", "INFO");
    } else {
        LOG("Bot " + clientId + " - Avertissement : Le thread du bot semble déjà démarré.", "WARNING");
    }
}

// --- Implémentation stop() ---
void Bot::stop() {
    bool expected = true;
    // Utilise compare_exchange_strong pour s'assurer que running était true avant de le mettre à false
    if (running.compare_exchange_strong(expected, false)) {
        // running a été mis à false, le thread devrait s'arrêter bientôt.
        LOG("Bot " + clientId + " - Signal d'arrêt envoyé au thread du bot.", "INFO");
        // Attend que le thread se termine (quitte sa boucle)
        if (botThread.joinable()) {
            botThread.join();
            LOG("Bot " + clientId + " - Thread du bot arrêté proprement.", "INFO");
        } else {
             LOG("Bot " + clientId + " - Avertissement : Thread du bot non joignable à l'arrêt (peut-être jamais démarré ou déjà terminé).", "WARNING");
        }
    } else {
        // Le bot ne tournait pas (running était déjà false).
        // S'assurer que si le thread a été créé mais s'est arrêté tout seul (exception non gérée?), on ne plante pas sur join().
        // Un check joinable() avant join() est essentiel.
         if (botThread.joinable()) {
             // Le thread a pu s'arrêter tout seul. On join quand même pour nettoyer les ressources du thread.
             // Cela ne devrait normalement pas arriver avec une gestion propre des exceptions dans tradeLoop.
             LOG("Bot " + clientId + " - Avertissement : Thread du bot était joinable mais running était déjà false. Join for cleanup.", "WARNING");
             botThread.join();
         } else {
            LOG("Bot " + clientId + " - Avertissement : Le thread du bot ne semble pas démarré (ou déjà terminé et joint). stop() n'a rien fait.", "WARNING");
         }
    }
}

// --- Implémentation tradeLoop() : La boucle principale du bot ---
void Bot::tradeLoop() {
    LOG("Bot " + clientId + " - Thread de logique de trading démarré.", "INFO");

    // Boucle tant que le flag 'running' est vrai.
    // running.load() lit la valeur de l'atomic bool de manière thread-safe.
    while (running.load()) {
        // Utilisez un try-catch pour éviter qu'une exception non gérée dans la boucle ne fasse crasher tout le serveur.
        try {
            // LOG("Bot " + clientId + " - Boucle de trading : Début itération.", "DEBUG"); // Supprimé (DEBUG)

            // --- 1. Exécuter la logique de décision ---
            // processLatestPrice() calcule les indicateurs et décide de l'action.
            // Cette méthode utilise botMutex en interne pour protéger l'accès à l'historique/état du bot.
            TradingAction action = processLatestPrice();

            // --- 2. Si une action de trading est décidée (pas HOLD ou UNKNOWN) ---
            if (action != TradingAction::HOLD &&
                action != TradingAction::UNKNOWN) // Ne soumet pas HOLD ou UNKNOWN
                 {
                // Action décidée (BUY ou CLOSE_LONG dans ce code simple)
                // LOG("Bot " + clientId + " - Boucle de trading : Action décidée : " + tradingActionToString(action) + ". Tentative de soumission d'ordre.", "DEBUG"); // Supprimé (DEBUG)

                // --- 3. Obtenir un pointeur fort vers la ClientSession et soumettre l'ordre ---
                // On utilise le weak_ptr stocké pour tenter d'obtenir un shared_ptr.
                // Si la ClientSession a été détruite entre-temps, le lock() échouera et retournera un shared_ptr nullptr.
                std::shared_ptr<ClientSession> session_sp = clientSessionPtr.lock(); // Tente de lock le weak_ptr

                if (session_sp) {
                    // La ClientSession est toujours en vie !
                    // On peut appeler submitBotOrder. submitBotOrder forme la TransactionRequest et l'ajoute à la TQ.
                    session_sp->submitBotOrder(action); // Appelle la méthode de la ClientSession (ne retourne plus de bool)

                    // Si submitBotOrder n'est pas sorti prématurément (à cause de wallet manquant, etc., loggé à l'intérieur),
                    // on peut considérer que la soumission *à la TQ* a été tentée/réussie.
                    // Le résultat réel de la transaction sera notifié plus tard via applyTransactionRequest.
                    LOG("Bot " + clientId + " - Boucle de trading : Tentative de soumission d'ordre '" + tradingActionToString(action) + "'. Résultat final via notification.", "INFO");

                    // L'ancien 'else' pour échec de soumission immédiat est supprimé,
                    // car submitBotOrder gère désormais ses échecs préliminaires en interne (logs et return;).

                    // Dans votre conception actuelle, le bot est mis à jour par notifyTransactionCompleted
                    // quand le résultat de TQ arrive pour CETTE transaction.

                } else {
                    LOG("Bot " + clientId + " - Boucle de trading : ClientSession n'est plus disponible (expired weak_ptr). Impossible de soumettre l'ordre. Arrêt du bot.", "WARNING");
                     // Si la ClientSession n'est plus là, le bot ne peut plus soumettre d'ordres. Il doit s'arrêter.
                     running.store(false); // Signal l'arrêt propre
                }

            } else { // Action HOLD ou UNKNOWN
                 // Action HOLD ou UNKNOWN, pas de soumission d'ordre.
                 // LOG("Bot " + clientId + " - Boucle de trading : Action HOLD ou non soumise (" + tradingActionToString(action) + ").", "DEBUG"); // Supprimé (DEBUG)
            }

            // LOG("Bot " + clientId + " - Boucle de trading : Fin itération. Avant pause.", "DEBUG"); // Supprimé (DEBUG)

            // --- 4. Pause avant la prochaine itération ---
            // Le thread s'endort pour l'intervalle défini.
            // Utilise running.load() dans la condition d'une boucle interne pour pouvoir sortir plus tôt si stop() est appelé pendant le sleep.
            // Cela rend le stop plus réactif.
            auto start_sleep = std::chrono::steady_clock::now();
            while (running.load() && std::chrono::steady_clock::now() - start_sleep < tradeInterval) {
                 std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Dormir par petits intervalles réactifs
            }
            // Si running devient false pendant le sleep, la boucle while interne s'arrête,
            // et la boucle while(running.load()) principale s'arrêtera à la prochaine vérification.

        } catch (const std::exception& e) {
            // Attrape les exceptions standards qui pourraient survenir dans la boucle (processLatestPrice, soumission, etc.)
            LOG("Bot " + clientId + " - Boucle de trading : Exception non gérée dans l'itération. Bot va s'arrêter. Erreur: " + std::string(e.what()), "ERROR");
            running.store(false); // Signale l'arrêt du bot en cas d'exception
        } catch (...) {
             // Attrape toute autre exception inconnue
             LOG("Bot " + clientId + " - Boucle de trading : Exception inconnue non gérée dans l'itération. Bot va s'arrêter.", "ERROR");
             running.store(false); // Signale l'arrêt du bot
        }

    } // Fin while(running.load())

    LOG("Bot " + clientId + " - Thread de logique de trading arrêté proprement.", "INFO");
}

// --- Implémentation des Getters (Thread-safe) ---

PositionState Bot::getCurrentState() const {
    std::lock_guard<std::mutex> lock(botMutex); // Protège l'accès
    return currentState;
}

double Bot::getEntryPrice() const {
    std::lock_guard<std::mutex> lock(botMutex); // Protège l'accès
    return entryPrice;
}

const std::string& Bot::getClientId() const {
    return clientId; // Membre const, pas de modification possible
}

std::shared_ptr<Wallet> Bot::getClientWallet() const {
    // Retourne une copie du shared_ptr.
    // L'accès aux méthodes du Wallet pointé (ex: getBalance) DOIT être thread-safe via ses locks INTERNES,
    // car ces méthodes peuvent être appelées par des threads (comme ce bot) qui ne détiennent pas le lock externe de la TQ.
    return clientWallet; // Le shared_ptr lui-même peut être retourné atomiquement.
}


// --- Implémentation de processLatestPrice() : La logique de décision ---
// Appelé par tradeLoop. Protégé par botMutex.
TradingAction Bot::processLatestPrice() {
    // Protège l'accès aux membres mutables (historique, état, prix d'entrée)
    std::lock_guard<std::mutex> lock(botMutex);

    LOG("Bot " + clientId + " - Traitement du dernier prix...", "INFO");

    // 1. Obtenir le dernier prix.
    double latestPrice = Global::getPrice("SRD-BTC"); // Accès thread-safe à Global
    if (latestPrice <= 0 || !std::isfinite(latestPrice)) {
        std::stringstream ss_log;
        ss_log << "Bot " << clientId << " - Avertissement: Prix invalide (" << std::fixed << std::setprecision(10) << latestPrice << "). HOLD.";
        LOG(ss_log.str(), "WARNING");
        return TradingAction::HOLD;
    }

    // 2. Mettre à jour et limiter l'historique.
    priceHistoryForCalculation.push_back(latestPrice);
    size_t max_history_size = static_cast<size_t>(bollingerPeriod) * 2; // Garder assez de données pour les calculs + un peu plus
    if (priceHistoryForCalculation.size() > max_history_size) {
        priceHistoryForCalculation.erase(
            priceHistoryForCalculation.begin(),
            priceHistoryForCalculation.begin() + (priceHistoryForCalculation.size() - max_history_size)
        );
        // LOG("Bot " + clientId + " - Historique tronqué. Taille: " + std::to_string(priceHistoryForCalculation.size()), "DEBUG"); // Supprimé (DEBUG)
    }

    // 3. Calculer les Bandes de Bollinger.
    size_t required_size = static_cast<size_t>(bollingerPeriod);
    if (priceHistoryForCalculation.size() < required_size) {
        LOG("Bot " + clientId + " - Pas assez de données pour Bandes (" +
            std::to_string(priceHistoryForCalculation.size()) + "/" + std::to_string(bollingerPeriod) + "). HOLD.", "INFO");
        return TradingAction::HOLD;
    }

    BollingerBands bands = calculateBands(); // Appelle calculateBands (appelée sous botMutex)
    std::stringstream ss_log_bands;
    ss_log_bands << "Bot " << clientId << " - Prix : " << std::fixed << std::setprecision(10) << latestPrice
                 << ", Bandes (M: " << std::fixed << std::setprecision(10) << bands.middleBand
                 << ", U: " << std::fixed << std::setprecision(10) << bands.upperBand
                 << ", L: " << std::fixed << std::setprecision(10) << bands.lowerBand << ")";
    LOG(ss_log_bands.str(), "INFO");

    // --- 4. Logique de Trading (Bollinger simple) ---
    TradingAction action = TradingAction::HOLD;

    // Vérifier si on a accès au Wallet pour vérifier les soldes si nécessaire pour la décision
    if (!clientWallet) {
         LOG("Bot " + clientId + " - WARNING: Wallet non disponible pour prendre une décision. HOLD.", "WARNING");
         return TradingAction::HOLD;
    }

    if (currentState == PositionState::NONE) {
        // Si sans position, chercher un signal d'achat sous la bande inférieure
        if (latestPrice <= bands.lowerBand) {
             // Vérifier si on a assez d'USD pour un BUY
             // L'accès au Wallet->getBalance DOIT être thread-safe.
            double current_usd_balance = clientWallet->getBalance(Currency::USD); // APPEL WALLET
            double amount_to_use_usd = current_usd_balance * (BOT_INVESTMENT_PERCENTAGE / 100.0);

            if (amount_to_use_usd > 0.0) { // Ne pas générer d'action BUY si le montant calculé est 0 ou moins
                 LOG("Bot " + clientId + " - Signal BUY (Prix <= Bande Inf.) & Solde USD > 0 calculé. Décision BUY.", "INFO");
                 action = TradingAction::BUY;
            } else {
                 LOG("Bot " + clientId + " - Signal BUY (Prix <= Bande Inf.) mais Solde USD insuffisant (" + std::to_string(current_usd_balance) + ") pour investir (" + std::to_string(amount_to_use_usd) + " calculé). HOLD.", "WARNING");
                 action = TradingAction::HOLD; // Pas assez d'USD pour acheter
            }

        }
    } else if (currentState == PositionState::LONG) {
        // Si en position LONG, chercher un signal de vente au-dessus de la bande supérieure
        if (latestPrice >= bands.upperBand) {
             // Vérifier si on a bien du SRD-BTC à vendre (solde > 0)
             // L'accès au Wallet->getBalance DOIT être thread-safe.
            double current_srd_btc_balance = clientWallet->getBalance(Currency::SRD_BTC); // APPEL WALLET

            if (current_srd_btc_balance > 0.0) { // Ne pas générer d'action SELL si le solde SRD-BTC est 0
                 LOG("Bot " + clientId + " - Signal CLOSE_LONG (Prix >= Bande Sup.) & Solde SRD-BTC > 0. Décision CLOSE_LONG.", "INFO");
                 action = TradingAction::CLOSE_LONG;
            } else {
                 LOG("Bot " + clientId + " - Signal CLOSE_LONG (Prix >= Bande Sup.) mais Solde SRD-BTC nul (" + std::to_string(current_srd_btc_balance) + "). Reste en LONG mais ne peut pas vendre.", "WARNING");
                 action = TradingAction::HOLD; // Signal de vente mais pas de crypto à vendre
            }
        }
    }
    // Logique pour l'état SHORT non implémentée.

    // Log l'action décidée.
    std::stringstream ss_log_action;
    ss_log_action << "Bot " << clientId << " - Action décidée: " << tradingActionToString(action);
    LOG(ss_log_action.str(), "INFO");

    return action; // Retourne l'action décidée.
}


// --- Implémentation notifyTransactionCompleted() ---
// Notification reçue lorsque l'application d'une transaction pour ce client bot est complétée.
// Met à jour l'état de position du bot. Thread-safe (appelé par ClientSession). Protégé par botMutex.
void Bot::notifyTransactionCompleted(const Transaction& tx) {
    // Protège l'accès aux membres mutables du bot (currentState, entryPrice)
    std::lock_guard<std::mutex> lock(botMutex);

    // Log la notification reçue.
    std::stringstream ss_log_tx;
    ss_log_tx << "Bot " << clientId << " - Notification transaction (ID: " << tx.getId()
              << ", Type: " << transactionTypeToString(tx.getType())
              << ", Statut: " << transactionStatusToString(tx.getStatus())
              << ", Client ID TX: " << tx.getClientId() << ")";
    LOG(ss_log_tx.str(), "INFO");


    // Vérifie si la transaction est COMPLETED/FAILED et si elle appartient bien à ce client bot.
    if (tx.getClientId() == this->clientId) {
        if (tx.getStatus() == TransactionStatus::COMPLETED) {
            // Gère la mise à jour de l'état en cas de succès.
            if (tx.getType() == TransactionType::BUY) {
                // Si un BUY a réussi
                if (currentState == PositionState::NONE) {
                    // Si on était sans position, on passe en LONG
                    currentState = PositionState::LONG;
                    entryPrice = tx.getUnitPrice(); // Enregistre le prix d'entrée de la transaction
                    std::stringstream ss_log_entry;
                    ss_log_entry << "Bot " << clientId << " - Position ouverte: LONG @ " << std::fixed << std::setprecision(10) << entryPrice;
                    LOG(ss_log_entry.str(), "INFO");
                } else if (currentState == PositionState::LONG) {
                     // Si on était déjà LONG, c'est un renforcement.
                     std::stringstream ss_log_reinforce;
                     ss_log_reinforce << "Bot " << clientId << " - LONG renforcée @ " << std::fixed << std::setprecision(10) << tx.getUnitPrice() << " (logique moyenne d'entrée à implémenter si nécessaire)";
                     LOG(ss_log_reinforce.str(), "INFO");
                     // Logique de calcul du prix d'entrée moyen pour les renforcements à implémenter si nécessaire.
                } else if (currentState == PositionState::SHORT) {
                    // Si on était SHORT, c'est une erreur logique ou une stratégie avancée non gérée ici.
                    LOG("Bot " + clientId + " - WARNING: Transaction BUY COMPLETED reçue alors que l'état est SHORT. Logique avancée non gérée.", "WARNING");
                }


            } else if (tx.getType() == TransactionType::SELL) {
                // Si un SELL a réussi
                if (currentState == PositionState::LONG) {
                    // Si on était LONG, on clôture la position
                     std::stringstream ss_log_close_long;
                     ss_log_close_long << "Bot " << clientId << " - Position LONG clôturée @ " << std::fixed << std::setprecision(10) << tx.getUnitPrice() << ". Entrée était @ " << std::fixed << std::setprecision(10) << entryPrice;
                     LOG(ss_log_close_long.str(), "INFO");
                    currentState = PositionState::NONE; // Retour à l'état neutre
                    entryPrice = 0.0; // Réinitialise le prix d'entrée
                } else if (currentState == PositionState::NONE) {
                     // Si on était sans position, un SELL réussi peut être une tentative d'ouvrir une position SHORT.
                     // Votre stratégie actuelle ne gère pas l'ouverture de SHORT.
                     std::stringstream ss_log_try_short;
                     ss_log_try_short << "Bot " << clientId << " - WARNING: Transaction SELL COMPLETED reçue alors que l'état est NONE. Tente d'ouvrir une position SHORT @ " << std::fixed << std::setprecision(10) << tx.getUnitPrice() << " (logique SHORT non implémentée).";
                     LOG(ss_log_try_short.str(), "WARNING");
                     // Si vous implémentez le short, c'est ici qu'il faudrait passer en état SHORT et enregistrer le prix d'entrée SHORT.
                } else if (currentState == PositionState::SHORT) {
                    // Si on était SHORT, c'est une clôture de SHORT.
                    std::stringstream ss_log_close_short;
                     ss_log_close_short << "Bot " + clientId << " - WARNING: Transaction SELL COMPLETED reçue alors que l'état est SHORT. Clôture SHORT @ " << std::fixed << std::setprecision(10) << tx.getUnitPrice() << " (logique clôture SHORT non implémentée).";
                     LOG(ss_log_close_short.str(), "WARNING");
                     // Si vous implémentez le short, c'est ici qu'il faudrait repasser en état NONE.
                }
            } else {
                 // Type de transaction COMPLETED non géré par la logique d'état du bot.
                 LOG("Bot " + clientId + " - WARNING: Transaction de type COMPLETED non géré par la logique d'état du bot. Type: " + transactionTypeToString(tx.getType()) + ", ID: " + tx.getId(), "WARNING");
            }

            // Log le nouvel état après traitement de la transaction COMPLETED
             std::stringstream ss_log_state;
             ss_log_state << "Bot " << clientId << " - Nouvel état après TX (ID: " << tx.getId() << ", Statut: COMPLETED): " << positionStateToString(currentState) << (currentState != PositionState::NONE ? (" @ " + std::to_string(entryPrice)) : "");
             LOG(ss_log_state.str(), "INFO");


        } else if (tx.getStatus() == TransactionStatus::FAILED) {
            // Gère l'échec de transaction. L'état du bot ne change pas en cas d'échec.
            std::stringstream ss_log_failed;
            ss_log_failed << "Bot " + clientId + " - Transaction (ID: " << tx.getId()
                          << ", Type: " << transactionTypeToString(tx.getType())
                          << ") ÉCHOUÉE. Raison: " << tx.getFailureReason();
            LOG(ss_log_failed.str(), "ERROR"); // Log niveau ERROR pour les échecs.
            // Logique de gestion d'erreur plus avancée si nécessaire (retry? ajuster stratégie?).
        }
         // Ignore les statuts PENDING ou UNKNOWN reçus (ne devraient pas arriver ici si ClientSession gère bien).
    }
     // Ignore les notifications de transactions qui ne concernent pas ce client (anormal si ça arrive).
}