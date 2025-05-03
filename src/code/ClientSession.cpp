#include "../headers/ClientSession.h"

// Assurez-vous que ces headers sont correctement inclus pour les classes utilisées
#include "../headers/ServerConnection.h" // Définition complète
#include "../headers/Bot.h"
#include "../headers/Wallet.h"
#include "../headers/Global.h" // Pour accès direct aux prix
#include "../headers/TransactionQueue.h" // Pour ajouter requêtes et accéder à txQueue globale
#include "../headers/Logger.h" // Pour LOG macro
#include "../headers/Transaction.h" // Pour enums et struct TransactionRequest, et helpers stringTo/ToString
#include "../headers/TransactionQueue.h"

#include <iostream>
#include <sstream> // Pour le parsing des commandes et le formatage
#include <vector>
#include <limits> // Pour numeric_limits
#include <iomanip> // Pour std::fixed, std::setprecision
#include <thread> // Pour std::this_thread::sleep_for
#include <chrono> // Pour std::chrono
#include <algorithm> // Pour std::min, std::max, std::transform
#include <cctype> // Pour ::tolower
#include <string> // Inclure explicitement pour std::to_string si nécessaire selon le compilateur/standard
#include <cmath> // Pour std::isfinite


// Assurez-vous que l'instance globale de la file est déclarée dans UN SEUL fichier .cpp (souvent Server.cpp)

// Constante pour la fréquence d'appel du bot (légèrement > fréquence de prix)
const std::chrono::seconds BOT_CALL_INTERVAL(16);
// Taille du buffer de réception
const int RECEIVE_BUFFER_SIZE = 1024; // Taille typique pour lire des commandes texte


// Déclaration des méthodes privées pour que le compilateur les connaisse
// si elles sont appelées avant leur définition complète dans le fichier.
bool submitBotOrder(TradingAction action); // Cette déclaration libre semble incorrecte pour une méthode membre ClientSession


// --- Constructeur ---
ClientSession::ClientSession(const std::string& id, std::shared_ptr<ServerConnection> clientConn, std::shared_ptr<Wallet> wallet)
    // Initialise les membres dans l'ordre de déclaration
    : clientId(id),
      client(clientConn),
      clientWallet(wallet),
      bot(nullptr), // Initialisation du shared_ptr bot à nullptr
      running(false) // Initialisation du flag running à false
{
    LOG("ClientSession INFO : Initialisation pour client " + clientId, "INFO");
    // L'héritage de enable_shared_from_this est initialisé automatiquement.
    // Le thread n'est pas démarré dans le constructeur. La méthode start() est appelée par Server.
}
// --- Destructeur ---
ClientSession::~ClientSession() {

    LOG("ClientSession INFO : Destruction de la session pour client " + clientId, "INFO");

    // Demander l'arrêt et joindre le thread si nécessaire
    stop(); // Appelle la méthode stop(), qui loggue sa propre tentative d'arrêt/join.

    // Sauvegarder le portefeuille à la déconnexion (après que le thread de session ne l'utilise plus)
    if (clientWallet) {
        // La méthode saveToFile loggue déjà succès/échec.
        clientWallet->saveToFile();
        LOG("ClientSession INFO : Portefeuille sauvegardé pour " + clientId + " avant destruction.", "INFO");
    } else {
        LOG("ClientSession WARNING : Wallet null lors de la destruction de la session pour " + clientId + ". Sauvegarde impossible.", "WARNING");
    }
// Le pointeur shared_ptr<ClientConnection> sera détruit ici quand ClientSession est détruit.
// Sa destruction appelle le destructeur de ServerConnection qui ferme la socket/libère SSL.
}

// --- Démarre le thread de session ---
void ClientSession::start() {
    // Vérifie si la session n'est pas déjà démarrée ET si la connexion client est valide (pourquoi démarrer si pas connecté ?)
    if (!running.load() && client && client->isConnected()) {
        LOG("ClientSession INFO : Démarrage du thread de session pour client " + clientId, "INFO");

        running.store(true); // Met le flag running à true AVANT de lancer le thread

        // Lancer le thread de la session. Il exécutera la méthode run() de cette instance.
        sessionThread = std::thread(&ClientSession::run, this);

        // Si le thread a été lancé avec succès.
        if (sessionThread.joinable()) {
             LOG("ClientSession INFO : Thread de session démarré et joignable pour client " + clientId, "INFO");
        } else {
             LOG("ClientSession ERROR : Échec du lancement du thread de session pour client " + clientId, "ERROR");
             running.store(false); // Marquer comme non running si le thread n'a pas démarré.
        }


    } else {
         LOG("ClientSession WARNING : Impossible de démarrer la session pour client " + clientId + " (déjà en cours ou client déconnecté)", "WARNING");
    }
}

// --- Demande l'arrêt du thread de session ---
void ClientSession::stop() {
    if (running.load()) {
        LOG("ClientSession INFO : Attente de la fin du thread de session pour " + clientId, "INFO");
        running = false; // Signale au thread de s'arrêter

        // Joindre le thread pour attendre sa fin propre
        if (sessionThread.joinable()) {
            LOG("ClientSession INFO : Fermeture proactive de la connexion client pour débloquer le thread de session pour " + clientId, "INFO");
            sessionThread.join();
            LOG("ClientSession INFO : Thread de session terminé pour " + clientId, "INFO");
        }

        // Fermer explicitement la connexion réseau APRES que le thread de session ne l'utilise plus
        if (client && client->isConnected()) { // Vérifier si le client est toujours valide et connecté
            LOG("ClientSession WARNING : Thread de session non joignable pour " + clientId + " lors de l'arrêt.", "WARNING");
            client->closeConnection(); // Appeler la méthode de Client.h
        }

        // Désenregistrer la session de la TQ APRES que le thread est joint et la connexion fermée
        // pour éviter que la TQ essaie de notifier une session en cours d'arrêt ou déconnectée.
        txQueue.unregisterSession(clientId);
        LOG("ClientSession INFO : Session " + clientId + " désenregistrée de la TQ suite à l'arrêt.", "INFO");

    } else {
        LOG("ClientSession WARNING : La session " + clientId + " n'était pas en cours d'exécution lors de la demande d'arrêt.", "WARNING");
    }

}

// --- Boucle principale du thread de session ---
// Cette méthode est exécutée par le thread 'sessionThread'.
void ClientSession::run() {
    LOG("ClientSession INFO : Thread de session démarré pour client " + clientId, "INFO");

    char read_buffer[RECEIVE_BUFFER_SIZE]; // Buffer pour les données brutes reçues de la connexion.
    std::string command_buffer; // Buffer pour accumuler les données réseau et extraire les commandes complètes (terminées par '\n').
    std::chrono::system_clock::time_point last_bot_call_time = std::chrono::system_clock::now(); // Pour le bot (si implémenté).

    // La boucle principale du thread continue tant que le flag 'running' est true
    // ET que l'objet client (ServerConnection) est valide ET connecté.
    while (running.load() && client && client->isConnected()) {

        // --- 1. Gérer la réception et le buffering des données ---
        int bytes_received = 0;
        try {
             // Tente de lire des données depuis la connexion.
             // Cette fonction est bloquante si aucune donnée n'est disponible,
             // mais elle devrait se débloquer si la connexion est fermée proprement (retourne 0)
             // ou si une erreur survient (retourne < 0).
             bytes_received = client->receive(read_buffer, sizeof(read_buffer));
        } catch (const std::exception& e) {
             // Attrape les exceptions lancées par la méthode receive (ex: erreurs SSL).
             LOG("ClientSession ERROR : Erreur de réception pour client " + clientId + ": " + e.what(), "ERROR");
             running.store(false); // Signale l'arrêt de la boucle.
             break; // Sort de la boucle while.
        }

        if (bytes_received > 0) {
            // Des données ont été reçues. Ajouter au buffer d'accumulation.
            command_buffer.append(read_buffer, bytes_received);
            // Log le nombre d'octets reçus et la taille actuelle du buffer d'accumulation (DEBUG supprimé).

            // --- Extraire les commandes complètes du buffer (terminées par '\n') ---
            size_t newline_pos;
            // Boucle tant qu'un caractère de fin de ligne ('\n') est trouvé dans le buffer.
            while ((newline_pos = command_buffer.find('\n')) != std::string::npos) {
                // Extrait la commande complète jusqu'au '\n'.
                std::string complete_command = command_buffer.substr(0, newline_pos);
                // Retire la commande extraite et le '\n' du buffer d'accumulation.
                command_buffer.erase(0, newline_pos + 1);

                processClientCommand(complete_command); // Appelle la logique de traitement de commande

                // Après traitement d'une commande, vérifier si le flag 'running' a été mis à false
                // (ex: par une commande "QUIT" ou "stop session" gérée dans processClientCommand).
                if (!running.load()) break; // Si arrêt demandé, sortir de la boucle d'extraction de commandes.
            }
             // Les données partielles qui ne forment pas encore une ligne complète restent dans command_buffer.

        } else if (bytes_received == 0) {
            // receive retourne 0 lorsque le pair (le client) ferme la connexion proprement.
            LOG("ClientSession INFO : Déconnexion propre détectée pour client " + clientId + ". Socket FD: " + std::to_string(client ? client->getSocketFD() : -1), "INFO");
            running.store(false); // Signale l'arrêt de la boucle.
            // La boucle se terminera à la prochaine vérification de la condition while.
        } else { // bytes_received < 0
            // receive retourne une valeur négative pour des erreurs SSL (SSL_ERROR_WANT_READ/WRITE, etc.).
            // ServerConnection::receive loggue déjà le type d'erreur SSL si possible.
            LOG("ClientSession ERROR : Erreur détectée lors de la réception pour client " + clientId + ". Arrêt de la session. Code retour receive: " + std::to_string(bytes_received), "ERROR");
            running.store(false); // Signale l'arrêt de la boucle.
            // La boucle se terminera à la prochaine vérification de la condition while.
        }

         // Vérifier les conditions d'arrêt après chaque cycle de réception/traitement pour une sortie rapide.
         if (!running.load() || !client || !client->isConnected()) {
             break; // Sort de la boucle while principale.
         }


        // --- 2. Gérer l'appel périodique au bot et la soumission de ses ordres ---
        // Cette section ne sera exécutée que si le bot est associé à cette session (non null).
        if (bot) {
            auto now = std::chrono::system_clock::now();
            // Vérifier si l'intervalle d'appel du bot est écoulé.
            if (now - last_bot_call_time >= BOT_CALL_INTERVAL) {
                // LOG("ClientSession DEBUG : Appel de la logique du bot pour " + clientId, "DEBUG"); // Supprimé (DEBUG)
                // Appeler la méthode du bot pour qu'il prenne une décision.
                TradingAction bot_action = bot->processLatestPrice();

                // --- Traduire la décision du bot en ordre et soumettre à la TQ ---
                if (bot_action != TradingAction::HOLD && bot_action != TradingAction::UNKNOWN) {
                    LOG("ClientSession INFO : Bot " + clientId + " a décidé l'action: " + (bot_action == TradingAction::BUY ? "BUY" : (bot_action == TradingAction::CLOSE_LONG ? "CLOSE_LONG" : "AUTRE")), "INFO");
                    // submitBotOrder gère le calcul de quantité, la création de requête et la soumission TQ
                    submitBotOrder(bot_action);
                } else {
                    // Log si le bot a décidé de ne rien faire (commenté par défaut pour éviter spam log).
                }

                last_bot_call_time = now; // Mettre à jour le temps du dernier appel au bot.
            }
        }

        // --- 3. Gestion de la pause ---
        // Un petit sleep pour éviter une boucle très active (utilisant 100% CPU) si receive n'est pas bloquant
        // ou s'il n'y a rien à recevoir ou à traiter (bot inactif, pas de commandes).
        // Essentiel si receive est non bloquant ou a un timeout très court.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } // Fin de la boucle while (condition running || client valide/connecté devient fausse)

    // Ce log est exécuté juste après la sortie de la boucle while.
    LOG("ClientSession INFO : Sortie de la boucle de session pour client " + clientId + ". Raison: running=" + std::to_string(running.load()) + ", client_connected=" + std::to_string(client ? client->isConnected() : false), "INFO");
    // La destruction de l'objet ClientSession (quand le shared_ptr n'est plus référencé) appellera ~ClientSession()
    // qui appellera stop() pour la fin propre.
}


// --- Traite une commande reçue du client (string complète) ---
// Appelée par ClientSession::run() quand une commande complète (terminée par '\n') est extraite du buffer.
void ClientSession::processClientCommand(const std::string& command) {
    LOG("ClientSession INFO : Début traitement commande pour client " + clientId + " : '" + command + "'", "INFO");

    std::stringstream ss(command);
    std::string base_command;
    ss >> base_command;

    std::transform(base_command.begin(), base_command.end(), base_command.begin(), ::toupper);

    std::string response_message = "";

    // --- Parsing et Dispatch ---
    if (base_command == "QUIT") {
        LOG("ClientSession INFO : Commande QUIT reçue pour client " + clientId + ". Signalement de l'arrêt de la session.", "INFO");
        running.store(false);
        response_message = "OK: Disconnecting.\n";

    } else if (base_command == "SHOW") {
        std::string target;
        ss >> target;
        std::transform(target.begin(), target.end(), target.begin(), ::toupper);

        if (target == "WALLET") {
             LOG("ClientSession INFO : Commande SHOW WALLET reçue pour client " + clientId, "INFO");
             std::shared_ptr<Wallet> wallet = getClientWallet();

             if (wallet) {
                 double usd_balance = wallet->getBalance(Currency::USD);
                 double srd_btc_balance = wallet->getBalance(Currency::SRD_BTC);

                 std::stringstream response_ss;
                 response_ss << std::fixed << std::setprecision(10)
                             << "BALANCE USD: " << usd_balance
                             << ", SRD-BTC: " << srd_btc_balance
                             << "\n";
                 response_message = response_ss.str();

             } else {
                 LOG("ClientSession ERROR : Portefeuille (Wallet) non disponible pour client " + clientId + " lors de la commande SHOW WALLET.", "ERROR");
                 response_message = "ERROR: Internal server error (Wallet not available).\n";
             }

        } else if (target == "TRANSACTIONS") {
             LOG("ClientSession INFO : Commande SHOW TRANSACTIONS reçue pour client " + clientId, "INFO");

             std::shared_ptr<Wallet> wallet = getClientWallet();

             if (wallet) {
                  std::vector<Transaction> history = wallet->getTransactionHistory();
                  int display_count = 10;
                  int start_index = std::max(0, (int)history.size() - display_count);

                  std::stringstream resp_ss;
                  resp_ss << "TRANSACTION_HISTORY (Total: " << history.size() << ", Showing last " << (history.size() - start_index) << "):\n";
                  for (size_t i = start_index; i < history.size(); ++i) {
                      // Assurez-vous que Transaction::getDescription() existe
                      resp_ss << "- " << history[i].getDescription() << "\n";
                  }
                  response_message = resp_ss.str();

             } else {
                  LOG("ClientSession ERROR : Portefeuille (Wallet) non disponible pour client " + clientId + " lors de la commande SHOW TRANSACTIONS.", "ERROR");
                  response_message = "ERROR: Internal server error (Wallet not available).\n";
             }

        } else {
             LOG("ClientSession WARNING : Cible inconnue pour SHOW reçue pour client " + clientId + " : '" + command + "'", "WARNING");
             response_message = "ERROR: Unknown SHOW target. Use SHOW WALLET or SHOW TRANSACTIONS.\n";
        }

    } else if (base_command == "GET_PRICE") {
         std::string symbol;
         ss >> symbol;
         std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

         if (!symbol.empty()) {
              double price = Global::getPrice(symbol);
              if (price > 0 && std::isfinite(price)) {
                   std::stringstream resp_ss;
                   resp_ss << "PRICE " << symbol << " " << std::fixed << std::setprecision(8) << price << "\n";
                   response_message = resp_ss.str();
              } else {
                    LOG("ClientSession ERROR : Prix invalide (" + std::to_string(price) + ") obtenu de Global pour symbole " + symbol + " pour client " + clientId, "ERROR");
                    response_message = "ERROR: Could not retrieve valid price for " + symbol + ".\n";
              }
         } else {
              LOG("ClientSession WARNING : Symbole manquant pour commande GET_PRICE pour client " + clientId, "WARNING");
              response_message = "ERROR: Missing symbol for GET_PRICE. Use GET_PRICE <symbol>.\n";
         }

    // ========================================================================
    // === Bloc de commandes BUY/SELL/START BOT/STOP BOT ===
    // ========================================================================
    } else if (base_command == "BUY" || base_command == "SELL") {
         if (bot) {
             LOG("ClientSession WARNING : Refus commande manuelle " + base_command + " de client " + clientId + " : Bot actif.", "WARNING");
             response_message = "ERROR: Manual trading (" + base_command + ") is disabled while the bot is active. Please stop the bot first.\n";
         } else {
             std::string currency_str;
             double percentage = 0.0;
             ss >> currency_str >> percentage;
             std::transform(currency_str.begin(), currency_str.end(), currency_str.begin(), ::toupper);

             Currency trade_currency = stringToCurrency(currency_str);

             if (trade_currency != Currency::UNKNOWN && percentage > 0.0 && percentage <= 100.0 && ss && !ss.fail()) {
                  RequestType req_type = (base_command == "BUY") ? RequestType::BUY : RequestType::SELL;

                  handleClientTradeRequest(req_type, currency_str, percentage);

                  LOG("ClientSession INFO : Requête de trading manuelle (" + base_command + " " + currencyToString(trade_currency) + " " + std::to_string(percentage) + "%) reçue pour client " + clientId + ". Soumission à la TQ via handleClientTradeRequest.", "INFO");
                  response_message = "OK: Your " + base_command + " request has been submitted for processing.\n";

             } else {
                 LOG("ClientSession WARNING : Syntaxe/valeurs invalides pour commande " + base_command + " de client " + clientId + ": '" + command + "'. Arguments: Devise='" + currency_str + "', Pourcentage=" + std::to_string(percentage) + ".", "WARNING");
                 response_message = "ERROR: Invalid syntax or value for " + base_command + ". Use " + base_command + " <Currency> <Percentage (1-100)>.\n";
             }
         }

        } else if (base_command == "START") {
            std::string bot_keyword;
            ss >> bot_keyword;
            if (bot_keyword == "BOT") { // C'est bien "START BOT"
                double bollingerK;
                // Tenter de lire le paramètre BollingerK (un double)
                if (ss >> bollingerK) {
                    // --- Vérifier s'il y a des paramètres non attendus après K ---
                    std::string remaining;
                    ss >> remaining; // Tente de lire quelque chose d'autre après le double
                    if (!remaining.empty()) { // S'il reste du texte après le nombre (ex: START BOT 2.0 texte)
                        response_message = "ERROR: Invalid command format for START BOT. Usage: START BOT <BollingerK>.\n";
                        LOG("Server WARNING : Commande START BOT avec texte en trop après K de client " + clientId + ". Commande: '" + command + "'", "WARNING");
                    }
                    else {
                        // --- Paramètre K lu avec succès et pas de texte en trop ! ---
                        // Appeler la méthode startBot avec le paramètre K.
                        // La période Bollinger (20) est gérée à l'intérieur de ClientSession::startBot.
                        startBot(bollingerK); // Appel de startBot
                        LOG("Server INFO : Commande START BOT reçue et parsée avec K=" + std::to_string(bollingerK) + " pour client " + clientId, "INFO");
                        // La méthode startBot envoie elle-même le message de confirmation ("BOT STARTED...").
                        // response_message est vide ici si startBot réussit (startBot envoie la réponse OK).
                    }
                } else {
                    // Le paramètre K n'est pas un nombre valide ou est manquant
                    response_message = "ERROR: Invalid or missing BollingerK value. Usage: START BOT <BollingerK> (e.g., 2.0).\n";
                    LOG("Server WARNING : Commande START BOT avec paramètre K invalide (pas un nombre ou manquant) de client " + clientId + ". Commande: '" + command + "'", "WARNING");
                }
            } else {
                // Juste "START" sans "BOT" ou avec un autre mot
                response_message = "ERROR: Unknown START command target '" + bot_keyword + "'. Available: START BOT <BollingerK>.\n";
                 LOG("Server WARNING : Commande START inconnue de client " + clientId + ". Commande: '" + command + "'", "WARNING");
            }
        } else if (base_command == "STOP") {
             std::string bot_keyword;
             ss >> bot_keyword;
             std::transform(bot_keyword.begin(), bot_keyword.end(), bot_keyword.begin(), ::toupper);

             if (bot_keyword == "BOT") { // C'est bien "STOP BOT"
                 // Vérifier s'il y a des paramètres inattendus
                 std::string remaining;
                 ss >> remaining;
                 if (!remaining.empty()) {
                      response_message = "ERROR: Invalid command format for STOP BOT. Usage: STOP BOT.\n";
                      LOG("Server WARNING : Commande STOP BOT avec texte en trop de client " + clientId + ". Commande: '" + command + "'", "WARNING");
                 } else {
                     // Commande valide "STOP BOT"
                     stopBot(); // Appel de stopBot
                     LOG("Server INFO : Commande STOP BOT reçue et parsée pour client " + clientId, "INFO");
                     // La méthode stopBot envoie elle-même le message de confirmation ("BOT STOPPED.").
                     // response_message est vide ici si stopBot réussit (stopBot envoie la réponse OK).
                 }
             } else {
                  // Juste "STOP" sans "BOT" ou avec un autre mot
                  response_message = "ERROR: Unknown STOP command target '" + bot_keyword + "'. Available: STOP BOT.\n";
                  LOG("Server WARNING : Commande STOP inconnue de client " + clientId + ". Commande: '" + command + "'", "WARNING");
             }

    // ========================================================================
    // === Fin du bloc de commandes BUY/SELL/START BOT/STOP BOT ===
    // ========================================================================

    } else { // Gérer les commandes inconnues
        LOG("ClientSession WARNING : Commande inconnue reçue pour client " + clientId + " : '" + command + "'", "WARNING");
        response_message = "ERROR: Unknown command '" + command + "'. Use SHOW WALLET, SHOW TRANSACTIONS, GET_PRICE <symbol>, BUY/SELL <Currency> <Percentage>, START BOT <BollingerK>, STOP BOT, or QUIT.\n";
    }

    // --- Envoyer le message de réponse au client ---
    if (!response_message.empty() && client && client->isConnected()) {
         try {
            client->send(response_message);
            // LOG("ClientSession DEBUG : Réponse envoyée à " + clientId + ": '" + response_message.substr(0, std::min(response_message.size(), (size_t)200)) + ((response_message.size() > 200) ? "..." : "") + "'", "DEBUG"); // Supprimé (DEBUG)
        } catch (const std::exception& e) {
             LOG("ClientSession ERROR : Erreur lors de l'envoi de la réponse à " + clientId + ": " + e.what(), "ERROR");
        }
    }

    LOG("ClientSession INFO : Fin traitement commande pour client " + clientId + " : '" + command + "'", "INFO");
}

// --- Implémentation de handleClientTradeRequest (pour les trades BASÉS SUR POURCENTAGE) ---
// Gère les demandes de transaction client (calcul, création TransactionRequest, soumission TQ)
// pour les commandes spécifiées en POURCENTAGE (ex: BUY SRD-BTC 10%).
// Le paramètre 'value' EST INTERPRETÉ comme un pourcentage (ex: 10 pour 10%).
// Ne fait PLUS de vérification de fonds préliminaire ici, la TQ le fera.
// Retourne true si soumission TQ réussie, false sinon.
bool ClientSession::handleClientTradeRequest(RequestType req_type, const std::string& cryptoName, double value_as_percentage) {
    double percentage = value_as_percentage;


    if (!clientWallet) {
        LOG("ClientSession ERROR : Portefeuille non disponible pour client " + clientId + " pour requête " + requestTypeToString(req_type) + " " + cryptoName + " " + std::to_string(percentage) + "%.", "ERROR");
        if (client && client->isConnected()) client->send("ERROR: Wallet not available for this operation.\n");
        return false;
    }

    // Vérifier la validité du nom de la crypto fournie.
    Currency trade_currency_enum = stringToCurrency(cryptoName);
    if (trade_currency_enum == Currency::UNKNOWN) {
         LOG("ClientSession WARNING : Devise inconnue spécifiée dans la requête de trading pour client " + clientId + ": '" + cryptoName + "'.", "WARNING");
         if (client && client->isConnected()) client->send("ERROR: Unknown currency specified: " + cryptoName + ".\n");
         return false;
    }

     // S'assurer que la requête est pour la paire/type supporté (ex: BUY/SELL SRD-BTC).
     if (trade_currency_enum != Currency::SRD_BTC || (req_type != RequestType::BUY && req_type != RequestType::SELL)) {
        LOG("ClientSession WARNING : Type de requête (" + requestTypeToString(req_type) + ") ou devise (" + cryptoName + ") non supporté par handleClientTradeRequest (pourcentage) pour client " + clientId + ".", "WARNING");
        if (client && client->isConnected()) client->send("ERROR: Only BUY/SELL of SRD-BTC is supported via client command (percentage).\n");
        return false;
    }

    // Déterminer la devise dont on prend un pourcentage du solde.
    Currency balance_currency_for_percentage = (req_type == RequestType::BUY) ? Currency::USD : Currency::SRD_BTC;

    // Obtenir le solde actuel pour calculer le montant voulu par le client.
    // L'appel getBalance DOIT être thread-safe en interne du Wallet.
    double current_balance_for_calc = clientWallet->getBalance(balance_currency_for_percentage);

    // --- Calculer le montant base basé sur le pourcentage ---
    double amount_based_on_percentage = current_balance_for_calc * (percentage / 100.0);

    // Vérifier le pourcentage ou montant calculé - doit être positif.
     if (percentage <= 0 || amount_based_on_percentage <= 0) {
        LOG("ClientSession WARNING : " + clientId + " - " + requestTypeToString(req_type) + " " + std::to_string(percentage) + "% " + cryptoName + " : Pourcentage (" + std::to_string(percentage) + ") ou montant calculé (" + std::to_string(amount_based_on_percentage) + ") nul ou négatif. Solde " + currencyToString(balance_currency_for_percentage) + ": " + std::to_string(current_balance_for_calc), "WARNING");
        if (client && client->isConnected()) client->send("ERROR: Percentage or calculated amount is zero or negative. Check balance and percentage.\n");
        return false;
    }


    // --- Calculer la quantité crypto visée pour la TransactionRequest ---
    double crypto_quantity_requested = 0.0;

    if (req_type == RequestType::BUY) {
        // Pour un BUY, le client veut utiliser un montant en USD (amount_based_on_percentage).
        // On calcule la quantité de crypto correspondante basée sur le prix ACTUEL (au moment de la commande).
        // Ce prix sera re-vérifié par la TQ au moment de l'exécution.
        double current_price_srd_btc = Global::getPrice(currencyToString(Currency::SRD_BTC)); // Obtenir prix (thread-safe)
        if (current_price_srd_btc <= 0 || !std::isfinite(current_price_srd_btc)) {
            LOG("ClientSession ERROR : Prix SRD-BTC non disponible ou invalide (" + std::to_string(current_price_srd_btc) + ") pour requête BUY (pourcentage) de client " + clientId, "ERROR");
            if (client && client->isConnected()) client->send("ERROR: Current price not available for BUY.\n");
            return false;
        }
        crypto_quantity_requested = amount_based_on_percentage / current_price_srd_btc;
        // LOG("ClientSession DEBUG : BUY (pourcentage) calculé (basé sur prix actuel " + std::to_string(current_price_srd_btc) + "): utiliser " + std::to_string(amount_based_on_percentage) + " USD (" + std::to_string(percentage) + "%) pour une quantité visée de " + std::to_string(crypto_quantity_requested) + " " + cryptoName + ".", "DEBUG"); // Supprimé (DEBUG)

    } else if (req_type == RequestType::SELL) {
        // Pour un SELL, le client veut vendre un pourcentage de sa crypto (amount_based_on_percentage).
        // amount_based_on_percentage est déjà la quantité de crypto visée.
        crypto_quantity_requested = amount_based_on_percentage;
        // LOG("ClientSession DEBUG : SELL (pourcentage) calculé : vendre " + std::to_string(crypto_quantity_requested) + " " + cryptoName + " (basé sur " + std::to_string(percentage) + "% de solde).", "DEBUG"); // Supprimé (DEBUG)
    }
    // Pas d'autre type de requête géré par cette fonction (pas de CLOSE_LONG/SHORT ici, c'est pour le bot)


   // --- Vérifier la quantité crypto visée finale ---
   if (crypto_quantity_requested <= 0) {
        LOG("ClientSession WARNING : " + clientId + " - " + requestTypeToString(req_type) + " " + std::to_string(percentage) + "% " + cryptoName + " : Quantité crypto calculée nulle ou négative (" + std::to_string(crypto_quantity_requested) + ").", "WARNING");
        if (client && client->isConnected()) client->send("ERROR: Calculated crypto quantity is zero or negative.\n");
        return false;
   }

    // --- Créer la requête de transaction ---
    // La TQ vérifiera les fonds réels et le prix au moment de l'exécution.
    TransactionRequest request(
        clientId,
        req_type,
        cryptoName, // Utilise le nom de la crypto (string) directement ici
        crypto_quantity_requested // La quantité (en crypto) calculée à trader
    );

    // Soumettre la requête à la file d'attente globale (TransactionQueue)
    extern TransactionQueue txQueue; // Accès à la TQ globale
    txQueue.addRequest(request); // txQueue.addRequest doit être thread-safe.

    // Log final de soumission.
    LOG("ClientSession INFO : Requête de transaction soumise à la TQ par client " + clientId + " (manuel, pourcentage) : Client=" + clientId + ", Type=" + requestTypeToString(req_type) + ", Qty Visée=" + std::to_string(crypto_quantity_requested) + " " + cryptoName + " (basé sur " + std::to_string(percentage) + "% de solde " + currencyToString(balance_currency_for_percentage) + ")", "INFO");

    return true; // Indique que la requête a été ajoutée à la TQ avec succès.
}


// --- submitBotOrder(TradingAction action) : Soumet un ordre décidé par le bot ---
// Cette méthode est appelée par le thread du Bot via le weak_ptr<ClientSession>.
// Elle traduit l'action du bot en une TransactionRequest et la soumet à la TQ.
// Ne fait PAS de vérification finale de fonds ou de prix ici, la TQ le fera.
void ClientSession::submitBotOrder(TradingAction action) {
    // Le bot doit exister pour que cette méthode soit appelée (via son weak_ptr).
    // Le Wallet doit aussi exister pour calculer les montants basés sur le solde.
    if (!clientWallet) {
        LOG("ClientSession ERROR : Portefeuille non disponible pour bot " + clientId + " pour action " + tradingActionToString(action) + ". Impossible de soumettre l'ordre.", "ERROR");
        // Ne pas envoyer d'erreur au client, c'est un problème interne du bot.
        return; // Sort de la fonction void en cas d'erreur préliminaire.
    }

    RequestType req_type = RequestType::UNKNOWN_REQUEST;
    Currency trade_currency = Currency::SRD_BTC; // Votre bot trade SRD-BTC

    double amount_to_trade_base = 0.0; // Montant dans la devise de base (USD pour BUY, SRD-BTC pour SELL)
    double percentage_used = 0.0; // Pourcentage utilisé si applicable (pour le log)
    double crypto_quantity_requested = 0.0; // Initialise la quantité crypto visée


    // --- Traduire l'action du bot en type de requête et calculer les montants ---
    // Ces calculs préliminaires déterminent la requête à soumettre, mais les vérifs finales sont dans la TQ.
    if (action == TradingAction::BUY) {
        req_type = RequestType::BUY;
        // Obtenir le solde USD actuel. L'appel getBalance DOIT être thread-safe en interne du Wallet.
        double current_usd_balance = clientWallet->getBalance(Currency::USD);
        // Utilise la constante globale définie dans Global.h
        percentage_used = BOT_INVESTMENT_PERCENTAGE;
        amount_to_trade_base = current_usd_balance * (percentage_used / 100.0);
        // LOG("ClientSession DEBUG : Bot " + clientId + " - BUY : Solde USD=" + std::to_string(current_usd_balance) + ", pour " + std::to_string(percentage_used) + "%, montant base=" + std::to_string(amount_to_trade_base) + " USD.", "DEBUG"); // Supprimé (DEBUG)

        // Pour un BUY, on a un montant en USD. On le convertit en quantité crypto visée en utilisant le prix actuel.
        // Ce prix sera re-vérifié par la TQ au moment de l'exécution.
        double current_price_srd_btc = Global::getPrice(currencyToString(Currency::SRD_BTC)); // Obtenir prix (thread-safe)

        // On ne soumet pas si les montants ou prix sont invalides au moment du calcul ici.
        if (amount_to_trade_base <= 0 || current_price_srd_btc <= 0 || !std::isfinite(current_price_srd_btc)) {
             LOG("ClientSession WARNING : " + clientId + " - BUY bot : Montant base (" + std::to_string(amount_to_trade_base) + ") ou prix SRD-BTC (" + std::to_string(current_price_srd_btc) + ") invalide(s). Annulation soumission.", "WARNING");
             return; // Sort en cas de données invalides.
         }

        crypto_quantity_requested = amount_to_trade_base / current_price_srd_btc;
        // LOG("ClientSession DEBUG : Bot " + clientId + " - BUY : Montant base USD " + std::to_string(amount_to_trade_base) + " converti en quantité crypto visée " + std::to_string(crypto_quantity_requested) + " @ prix " + std::to_string(current_price_srd_btc), "DEBUG"); // Supprimé (DEBUG)


    } else if (action == TradingAction::CLOSE_LONG) {
        req_type = RequestType::SELL;
        // Obtenir le solde SRD-BTC actuel. L'appel getBalance DOIT être thread-safe en interne du Wallet.
        double current_srd_btc_balance = clientWallet->getBalance(Currency::SRD_BTC);
        amount_to_trade_base = current_srd_btc_balance; // Vendre tout le solde SRD-BTC pour CLOSE_LONG
        percentage_used = 100.0; // Utilise 100% du solde SRD-BTC
        // LOG("ClientSession DEBUG : Bot " + clientId + " - CLOSE_LONG (SELL) : Solde SRD-BTC=" + std::to_string(current_srd_btc_balance) + ", pour " + std::to_string(percentage_used) + "%, quantité base=" + std::to_string(amount_to_trade_base) + " SRD-BTC.", "DEBUG"); // Supprimé (DEBUG)

        // Pour un SELL (CLOSE_LONG), amount_to_trade_base EST déjà la quantité de crypto visée.
        crypto_quantity_requested = amount_to_trade_base;

        // On ne soumet pas si la quantité est invalide au moment du calcul ici.
        if (crypto_quantity_requested <= 0) {
             LOG("ClientSession WARNING : " + clientId + " - CLOSE_LONG bot : Quantité crypto calculée nulle ou négative (" + std::to_string(crypto_quantity_requested) + "). Annulation soumission.", "WARNING");
             return; // Sort en cas de quantité invalide.
        }

    } else {
        // Si l'action est HOLD, UNKNOWN, ou un autre type non géré par cette fonction.
        if (action != TradingAction::HOLD && action != TradingAction::UNKNOWN) {
            LOG("ClientSession WARNING : Bot " + clientId + " a retourné une action non gérée pour soumission d'ordre : " + tradingActionToString(action) + ". Annulation soumission.", "WARNING");
        }
        // Pas de soumission pour ces cas.
        return; // Sort sans soumettre.
    }

    // --- Créer la TransactionRequest et l'ajouter à la TQ ---
    // Ces vérifications préliminaires ayant été passées, on procède à la soumission.
    // Les vérifications finales (fonds réels au moment de l'exécution, prix final) seront faites par la TQ.
    TransactionRequest request(
        clientId,             // ID du client (associé à cette session/bot)
        req_type,             // Type de requête (BUY/SELL)
        currencyToString(trade_currency), // Crypto concernée (SRD-BTC)
        crypto_quantity_requested // Quantité visée
    );

    // external TransactionQueue instance (déclarée extern dans ClientSession.h si elle n'est pas gérée autrement)
    extern TransactionQueue txQueue; // Accès à la TQ globale

    // txQueue.addRequest est thread-safe (la TQ gère sa propre file d'attente avec un mutex).
    txQueue.addRequest(request);

    LOG("ClientSession INFO : Requête de transaction soumise à la TQ par bot " + clientId + " (auto): Client=" + clientId + ", Type=" + requestTypeToString(req_type) + ", Qty Visée=" + std::to_string(crypto_quantity_requested) + " " + currencyToString(trade_currency), "INFO");

    // La fonction est void, il n'y a pas de return ici si la soumission a eu lieu.
 }


// --- applyTransactionRequest(const Transaction& tx) : Notification du résultat final de TQ ---
// Cette méthode est appelée par la TransactionQueue lorsqu'une transaction est appliquée pour ce client.
// Elle notifie le bot si présent, et envoie le résultat au client via la connexion réseau.
// Cette méthode est appelée depuis un thread de la TQ, elle DOIT être thread-safe (pas de modification des membres de ClientSession sans verrou si nécessaire).
void ClientSession::applyTransactionRequest(const Transaction& tx) {
    // Cette méthode est appelée par la TQ avec le résultat FINAL d'une transaction.

    // Log de la notification reçue.
    LOG("ClientSession INFO : Notification de TQ reçue pour transaction client " + clientId + ", ID: " + tx.getId() + ", Statut: " + transactionStatusToString(tx.getStatus()), "INFO");

    // Vérifie si la transaction concerne bien ce client (devrait être le cas si TQ l'appelle correctement)
    if (tx.getClientId() == this->clientId) {
        // Si un bot est associé à cette session, le notifier du résultat de la transaction.
        // La méthode notifyTransactionCompleted du bot est thread-safe en interne (elle utilise botMutex).
        if (bot) { // Si l'objet shared_ptr 'bot' n'est pas null
            // Appelle la méthode du bot pour qu'il mette à jour son état interne (PositionState, entryPrice).
            bot->notifyTransactionCompleted(tx);

            // LOG("ClientSession DEBUG : Bot actif pour client " + clientId + ". Notification de Tx " + tx.getId() + " passée au bot.", "DEBUG"); // Supprimé (DEBUG)
        } else {
             // Si pas de bot actif pour cette session, on ne le notifie pas.
            // LOG("ClientSession DEBUG : Bot non actif pour client " + clientId + ". Notification de Tx " + tx.getId() + " non passée au bot.", "DEBUG"); // Supprimé (DEBUG)
        }

         // --- Formater et envoyer le message de résultat au client ---
         // Le message TRANSACTION_RESULT est toujours envoyé au client, qu'il y ait un bot ou pas.
        std::stringstream result_msg_ss;
         result_msg_ss << "TRANSACTION_RESULT ID=" << tx.getId()
                       << " STATUS=" << transactionStatusToString(tx.getStatus());
         if (tx.getStatus() == TransactionStatus::FAILED) {
             // Ajouter la raison de l'échec si le statut est FAILED.
             result_msg_ss << " REASON=\"" << tx.getFailureReason() << "\""; // Utilise getFailureReason() de la Transaction
         }

         // Log du message formaté avant envoi.
         // LOG("ClientSession DEBUG : Message TRANSACTION_RESULT formaté pour " + clientId + " Tx " + tx.getId() + ": '" + result_msg_ss.str() + "'", "DEBUG"); // Supprimé (DEBUG)


        // Envoyer la réponse au client via le shared_ptr client (ServerConnection).
        // Vérifier si la connexion est toujours active.
        if (client && client->isConnected()) {
            try {
                // LOG("ClientSession DEBUG : applyTransactionRequest: Tentative d'envoi de la réponse TRANSACTION_RESULT au client " + clientId + "...", "DEBUG"); // Supprimé (DEBUG)
                // La méthode send() de ServerConnection doit être thread-safe.
                client->send(result_msg_ss.str() + "\n"); // N'oubliez pas le terminateur de ligne '\n' !
                // LOG("ClientSession DEBUG : applyTransactionRequest: Réponse TRANSACTION_RESULT envoyée (ou appel send terminé sans exception) à " + clientId + ".", "DEBUG"); // Supprimé (DEBUG)
            } catch (const std::exception& e) {
                 // Gérer les erreurs d'envoi (connexion fermée, etc.).
                 LOG("ClientSession ERROR : applyTransactionRequest: Exception std::exception lors de l'envoi de la réponse TRANSACTION_RESULT à client " + clientId + " pour Tx " + tx.getId() + ": " + e.what(), "ERROR");
                 // Marquer la connexion pour fermeture si nécessaire.
                 if (client) client->markForClose();
            } catch (...) {
                 LOG("ClientSession ERROR : applyTransactionRequest: Exception inconnue lors de l'envoi de la réponse TRANSACTION_RESULT à client " + clientId + " pour Tx " + tx.getId() + ".", "ERROR");
                 if (client) client->markForClose();
            }
        } else {
             // Le client s'est déconnecté avant que la notification n'arrive.
             LOG("ClientSession WARNING : applyTransactionRequest: Impossible d'envoyer le résultat de TQ à client " + clientId + ", client déconnecté ou objet client invalide pour Tx " + tx.getId() + ".", "WARNING");
             // La déconnexion sera gérée ailleurs (sessionLoop ou Server).
        }
    } else {
        // Log si la transaction reçue ne concerne pas ce client (cas très anormal).
         LOG("ClientSession ERROR : Reçu notification de TQ pour transaction ID " + tx.getId() + " destinée au client " + tx.getClientId() + ", mais cette ClientSession gère le client " + this->clientId + ". Ignoré.", "ERROR");
    }

    // Pas de log de fin de fonction ici.
 }


// --- startBot(double bollingerK) : Démarre la logique du bot ---
// Appelée par processClientCommand suite à la commande "START BOT <K>".
// Crée un nouvel objet Bot et démarre son thread interne.
void ClientSession::startBot(double bollingerK) {
    // Vérifier si un bot est déjà actif pour ce client.
    if (bot) {
        LOG("ClientSession WARNING : Bot déjà actif pour client " + clientId + ", ignorer START BOT.", "WARNING");
        // Envoyer une erreur au client si la connexion est toujours active.
        if (client && client->isConnected()) client->send("ERROR: Bot is already running.\n");
        return; // Ne pas démarrer un nouveau bot si un existe déjà.
    }

    // --- Définir la période Bollinger ---
    int bollingerPeriod = 20; // Période Bollinger hardcodée à 20


    // --- Validation basique du paramètre K reçu ---
    // On peut valider K ici avant de créer le Bot.
     if (bollingerK <= 0.0 || !std::isfinite(bollingerK)) {
          std::stringstream ss_log_err;
          ss_log_err << "ClientSession ERROR : Valeur de BollingerK invalide (" << std::fixed << std::setprecision(2) << bollingerK << ") pour client " << clientId << ". Échec démarrage bot.";
          LOG(ss_log_err.str(), "ERROR");
          if (client && client->isConnected()) client->send("ERROR: Invalid BollingerK value provided. Must be a positive number (e.g., 2.0).\n");
          return; // Ne pas créer le Bot avec un paramètre invalide.
     }


    // --- Créer l'objet Bot ---
    // On utilise std::make_shared pour créer l'objet Bot.
    // On passe l'ID client, la période P, le K reçu, le shared_ptr du Wallet,
    // et une weak_ptr vers CETTE ClientSession (en utilisant shared_from_this()).
    // Le weak_ptr permet au Bot d'appeler submitBotOrder() sans créer de référence cyclique forte.
    try {
         // shared_from_this() retourne un shared_ptr. On le convertit en weak_ptr.
         bot = std::make_shared<Bot>(clientId, bollingerPeriod, bollingerK, clientWallet, std::weak_ptr<ClientSession>(shared_from_this()));

    } catch (const std::bad_weak_ptr& e) {
         // Cette exception peut arriver si shared_from_this() est appelé trop tôt.
         LOG("ClientSession CRITICAL : Échec d'obtention de shared_from_this pour créer le Bot (cas START BOT). La ClientSession doit être gérée par un shared_ptr avant d'appeler startBot. Erreur: " + std::string(e.what()), "CRITICAL");
         if (client && client->isConnected()) client->send("ERROR: Internal server error starting bot.\n");
         bot = nullptr; // S'assurer que le pointeur bot est null en cas d'échec.
         return;
    } catch (const std::exception& e) {
          // Attraper d'autres exceptions lors de la création du Bot (ex: allocation mémoire).
          LOG("ClientSession ERROR : Exception lors de la création de l'objet Bot pour client " + clientId + ": " + e.what(), "ERROR");
          if (client && client->isConnected()) client->send("ERROR: Failed to start bot due to creation error.\n");
          bot = nullptr; // S'assurer que le pointeur bot est null en cas d'échec.
          return;
    }


    // --- Démarrer la logique interne du Bot ---
    if (bot) { // Vérifier que la création a réussi.
         // Appelle la méthode start() du Bot pour lancer son thread de logique.
         bot->start();

         LOG("ClientSession INFO : Bot créé et démarré pour client " + clientId + " avec P=" + std::to_string(bollingerPeriod) + ", K=" + std::to_string(bollingerK), "INFO");
         // Confirmer au client que le bot a démarré.
         if (client && client->isConnected()) client->send("BOT STARTED with P=" + std::to_string(bollingerPeriod) + ", K=" + std::to_string(bollingerK) + ".\n");
    } else {
        // Si std::make_shared a retourné nullptr (très rare, sauf en cas d'exception gérée par le catch).
        LOG("ClientSession ERROR : Échec de création (pointeur null) de l'objet Bot pour client " + clientId, "ERROR");
        if (client && client->isConnected()) client->send("ERROR: Failed to start bot (creation returned null).\n");
    }
 }

 // --- stopBot() : Arrête la logique du bot ---
// Appelée par processClientCommand suite à la commande "STOP BOT", ou par ClientSession::stop().
void ClientSession::stopBot() {
    // Vérifier si un bot est actif.
    if (!bot) {
        LOG("ClientSession WARNING : Aucun bot actif pour client " + clientId + ", ignorer STOP BOT.", "WARNING");
        if (client && client->isConnected()) client->send("ERROR: No bot is running.\n");
        return; // Rien à arrêter si le bot n'existe pas.
    }

    // Signaler au bot de s'arrêter proprement et attendre que son thread se termine.
    // bot->stop() met le flag 'running' du bot à false et appelle join() sur son thread interne.
    bot->stop();

    // Une fois le thread du bot arrêté et joint, on peut détruire l'objet Bot
    // en réinitialisant le shared_ptr. Cela appelle le destructeur du Bot (~Bot()).
    bot = nullptr;
    LOG("ClientSession INFO : Bot arrêté pour client " + clientId, "INFO");
    // Confirmer au client que le bot est arrêté.
    if (client && client->isConnected()) client->send("BOT STOPPED.\n");
 }

// --- Getters simples ---
const std::string& ClientSession::getClientId() const { return clientId; }
std::shared_ptr<ServerConnection> ClientSession::getClientConnection() const { return client; }
std::shared_ptr<Wallet> ClientSession::getClientWallet() const { return clientWallet; }