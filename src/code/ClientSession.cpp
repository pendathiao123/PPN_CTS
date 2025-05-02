#include "../headers/ClientSession.h"

// Assurez-vous que ces headers sont correctement inclus pour les classes utilisées
#include "../headers/ServerConnection.h" // Définition complète
#include "../headers/Bot.h"
#include "../headers/Wallet.h"
#include "../headers/Global.h" // Pour accès direct aux prix
#include "../headers/TransactionQueue.h" // Pour ajouter requêtes et accéder à txQueue globale
#include "../headers/Logger.h" // Pour LOG macro
#include "../headers/Transaction.h" // Pour enums et struct TransactionRequest, et helpers stringTo/ToString
#include "../headers/TransactionQueue.h" // Pour RequestType et requestTypeToString

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
// extern TransactionQueue txQueue; // Déjà déclarée comme extern dans TransactionQueue.h

// Constante pour la fréquence d'appel du bot (légèrement > fréquence de prix)
const std::chrono::seconds BOT_CALL_INTERVAL(16);
// Taille du buffer de réception
const int RECEIVE_BUFFER_SIZE = 1024; // Taille typique pour lire des commandes texte

// Constante pour le pourcentage d'investissement du bot (par exemple, 100% du solde disponible)
// Ou une autre valeur si le bot n'investit pas toujours tout.
const double BOT_INVESTMENT_PERCENTAGE = 100.0; // Exemple : 100%


// Déclaration des méthodes privées pour que le compilateur les connaisse
// si elles sont appelées avant leur définition complète dans le fichier.
// (Elles étaient déjà déclarées dans le .h, mais c'est une bonne pratique ici).
// void run(); // Déclaré dans .h
// void processClientCommand(const std::string& request); // Déclaré dans .h
// bool handleClientTradeRequest(RequestType req_type, Currency trade_currency, double percentage); // Déclaré dans .h
// --- Nouvelle méthode pour la soumission d'ordres du bot ---
bool submitBotOrder(TradingAction action);


// --- Constructeur ---
ClientSession::ClientSession(std::shared_ptr<ServerConnection> client_ptr, const std::string& id, [[maybe_unused]] std::shared_ptr<Server> server, const std::string& dataDirPath)
    : clientId(id), client(client_ptr), bot(nullptr), running(false) // running est initialisé à false ici.
{
    // Correction des appels LOG (Source, Niveau, Message)
    LOG("ClientSession INFO : Initialisation pour client " + clientId, "INFO");

    // Création du Wallet. Le constructeur du Wallet appelle loadFromFile() UNE FOIS.
    clientWallet = std::make_shared<Wallet>(clientId, dataDirPath);

    // Pas besoin d'appeler loadFromFile() à nouveau ici, ni de logguer "Portefeuille chargé/Impossible de charger"
    // car le constructeur du Wallet l'a déjà fait et les logs de Server::processAuthRequest
    // indiquent déjà si l'utilisateur est NEW (nouveau portefeuille) ou SUCCESS (portefeuille chargé).

    // L'enregistrement auprès de la TQ et le démarrage du thread de session (mettant running à true)
    // DOIVENT se faire APRES que l'objet ClientSession a été créé en tant que shared_ptr
    // et que le pointeur a été stocké (ex: dans activeSessions du Server).
    // Donc, l'enregistrement et le démarrage se font dans la méthode start(), appelée PAR Server::HandleClient.
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
        LOG("ClientSession WARNING : Wallet null lors de la destruction de la session pour " + clientId + ". Sauvegarde impossible.", "WARNING"); // Correction LOG
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

        // Enregistrer la session auprès de la TransactionQueue ici, après que l'objet est géré par un shared_ptr.
        // Server::HandleClient appelle txQueue.registerSession APRÈS avoir appelé session->start().
        // Le meilleur endroit pour appeler registerSession est PEUT-ÊTRE dans HandleClient après la création de la session,
        // ou TQ::registerSession doit être capable de prendre la session même si son thread interne n'est pas encore démarré.
        // Le code actuel de HandleClient appelle registerSession AVANT session->start(). Conservons cette structure pour l'instant.
        // Le log bizarre "Session ... enregistrée auprès de la TQ" venait peut-être d'ici ou d'un log dans TQ::registerSession ?
        // Le log dans TQ::registerSession semblait correct. Vérifions si ce log bizarre disparaît avec la correction des autres logs.
        // Si le log bizarre persiste, il faudra regarder où il est généré exactement.

        // Lancer le thread de la session. Il exécutera la méthode run() de cette instance.
        sessionThread = std::thread(&ClientSession::run, this);

        // Si le thread a été lancé avec succès.
        if (sessionThread.joinable()) {
             LOG("ClientSession INFO : Thread de session démarré et joignable pour client " + clientId, "INFO");
        } else {
             LOG("ClientSession ERROR : Échec du lancement du thread de session pour client " + clientId, "ERROR");
             running.store(false); // Marquer comme non running si le thread n'a pas démarré.
             // Devrait aussi fermer la connexion ici si le thread ne démarre pas ?
             // La destruction de ClientSession le fera, mais une gestion d'erreur plus fine serait mieux.
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
            // Si le thread est bloqué dans client->receive(), il faudrait un moyen de le débloquer
            // (ex: fermer le socket depuis un autre thread, ou utiliser des I/O non bloquantes/timeout).
            // Ici, on suppose que la boucle finira par se terminer (soit receive retourne 0, soit le flag running est vérifié périodiquement).
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

// --- Vérifie si le thread est actif ---
bool ClientSession::isRunning() const {
    return running.load();
}

// --- Boucle principale du thread de session ---
// Cette méthode est exécutée par le thread 'sessionThread'.
void ClientSession::run() {
    // Correction appel LOG : format (Message, Niveau)
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
             LOG("ClientSession ERROR : Erreur de réception pour client " + clientId + ": " + e.what(), "ERROR"); // Correction LOG
             running.store(false); // Signale l'arrêt de la boucle.
             break; // Sort de la boucle while.
        }

        if (bytes_received > 0) {
            // Des données ont été reçues. Ajouter au buffer d'accumulation.
            command_buffer.append(read_buffer, bytes_received);
            // Log le nombre d'octets reçus et la taille actuelle du buffer d'accumulation.
            LOG("ClientSession DEBUG : Reçu " + std::to_string(bytes_received) + " octets. Buffer d'accumulation: " + std::to_string(command_buffer.size()) + " octets.", "DEBUG"); // Correction LOG


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
            LOG("ClientSession INFO : Déconnexion propre détectée pour client " + clientId + ". Socket FD: " + std::to_string(client ? client->getSocketFD() : -1), "INFO"); // Correction LOG + ajout FD
            running.store(false); // Signale l'arrêt de la boucle.
            // La boucle se terminera à la prochaine vérification de la condition while.
        } else { // bytes_received < 0
            // receive retourne une valeur négative pour des erreurs SSL (SSL_ERROR_WANT_READ/WRITE, etc.).
            // ServerConnection::receive loggue déjà le type d'erreur SSL si possible.
            LOG("ClientSession ERROR : Erreur détectée lors de la réception pour client " + clientId + ". Arrêt de la session. Code retour receive: " + std::to_string(bytes_received), "ERROR"); // Correction LOG + code retour
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
                LOG("ClientSession DEBUG : Appel de la logique du bot pour " + clientId, "DEBUG"); // Correction LOG
                // Appeler la méthode du bot pour qu'il prenne une décision.
                // processLatestPrice() doit être implémenté dans la classe Bot.
                TradingAction bot_action = bot->processLatestPrice(); // TODO: Implement Bot::processLatestPrice()

                // --- Traduire la décision du bot en ordre et soumettre à la TQ ---
                if (bot_action != TradingAction::HOLD) {
                    LOG("ClientSession INFO : Bot " + clientId + " a décidé l'action: " + (bot_action == TradingAction::BUY ? "BUY" : (bot_action == TradingAction::CLOSE_LONG ? "CLOSE_LONG" : "AUTRE")), "INFO"); // Correction LOG
                    // submitBotOrder gère le calcul de quantité, la création de requête et la soumission TQ
                    // submitBotOrder(bot_action); // TODO: Implement submitBotOrder()
                } else {
                    // Log si le bot a décidé de ne rien faire (commenté par défaut pour éviter spam log).
                    // LOG("ClientSession DEBUG : Bot " + clientId + " a décidé: HOLD.", "DEBUG"); // Correction LOG
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
    // Correction appel LOG : format (Message, Niveau)
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

                  handleClientTradeRequest(req_type, trade_currency, percentage);

                  LOG("ClientSession INFO : Requête de trading manuelle (" + base_command + " " + currencyToString(trade_currency) + " " + std::to_string(percentage) + "%) reçue pour client " + clientId + ". Soumission à la TQ via handleClientTradeRequest.", "INFO");
                  response_message = "OK: Your " + base_command + " request has been submitted for processing.\n";

             } else {
                 LOG("ClientSession WARNING : Syntaxe/valeurs invalides pour commande " + base_command + " de client " + clientId + ": '" + command + "'. Arguments: Devise='" + currency_str + "', Pourcentage=" + std::to_string(percentage) + ".", "WARNING");
                 response_message = "ERROR: Invalid syntax or value for " + base_command + ". Use " + base_command + " <Currency> <Percentage (1-100)>.\n";
             }
         }

    } else if (base_command == "START") {
         std::string bot_command;
         ss >> bot_command;
         std::transform(bot_command.begin(), bot_command.end(), bot_command.begin(), ::toupper);
         if (bot_command == "BOT") {
             int period = 0;
             double k = 0.0;
             ss >> period >> k;
              if (ss && !ss.fail() && period > 0 && k > 0.0) {
                 startBot(period, k);

                 response_message = ""; // startBot gère l'envoi de réponse

              } else {
                 LOG("ClientSession WARNING : Syntaxe/valeurs invalides pour START BOT de client " + clientId + ": '" + command + "'. Arguments: Period=" + std::to_string(period) + ", K=" + std::to_string(k) + ".", "WARNING");
                 response_message = "ERROR: Invalid syntax or values for START BOT. Use START BOT <period> <K>.\n";
              }
         } else {
             LOG("ClientSession WARNING : Commande START inconnue de client " + clientId + ": '" + command + "'", "WARNING");
             response_message = "ERROR: Unknown START command. Use START BOT.\n";
         }

    } else if (base_command == "STOP") {
         std::string stop_command;
         ss >> stop_command;
         std::transform(stop_command.begin(), stop_command.end(), stop_command.begin(), ::toupper);
         if (stop_command == "BOT") {
             stopBot();

             response_message = ""; // stopBot gère l'envoi de réponse

         } else if (stop_command == "SESSION") {
              LOG("ClientSession INFO : Commande STOP SESSION reçue pour client " + clientId + ". Signalement de l'arrêt de la session.", "INFO");
              running.store(false);
              response_message = "OK: Stopping session.\n";
         }
           else {
               LOG("ClientSession WARNING : Commande STOP inconnue de client " + clientId + ": '" + command + "'", "WARNING");
               response_message = "ERROR: Unknown STOP command. Use STOP BOT (or STOP SESSION).\n";
           }
    // ========================================================================
    // === Fin du bloc de commandes BUY/SELL/START BOT/STOP BOT ===
    // ========================================================================

    } else { // Gérer les commandes inconnues
        LOG("ClientSession WARNING : Commande inconnue reçue pour client " + clientId + " : '" + command + "'", "WARNING");
        response_message = "ERROR: Unknown command '" + command + "'. Use SHOW WALLET, SHOW TRANSACTIONS, GET_PRICE <symbol>, BUY/SELL <Currency> <Percentage>, START BOT <period> <K>, STOP BOT, STOP SESSION, or QUIT.\n";
    }

    // --- Envoyer le message de réponse au client ---
    if (!response_message.empty() && client && client->isConnected()) {
         try {
            client->send(response_message);
            LOG("ClientSession DEBUG : Réponse envoyée à " + clientId + ": '" + response_message.substr(0, std::min(response_message.size(), (size_t)200)) + ((response_message.size() > 200) ? "..." : "") + "'", "DEBUG");
        } catch (const std::exception& e) {
             LOG("ClientSession ERROR : Erreur lors de l'envoi de la réponse à " + clientId + ": " + e.what(), "ERROR");
        }
    }

    LOG("ClientSession INFO : Fin traitement commande pour client " + clientId + " : '" + command + "'", "INFO");
}


// --- Implémentation Corrigée de handleClientTradeRequest ---
// Gère les demandes de transaction client (calcul, création TransactionRequest, soumission TQ).
// Ne fait PLUS de vérification de fonds préliminaire ici.
// Retourne true si soumission TQ réussie, false sinon.
bool ClientSession::handleClientTradeRequest(RequestType req_type, Currency trade_currency, double percentage) {
    if (!clientWallet) {
        LOG("ClientSession ERROR : Portefeuille non disponible pour client " + clientId + " pour requête " + requestTypeToString(req_type), "ERROR");
        if (client && client->isConnected()) client->send("ERROR: Wallet not available for this operation.\n");
        return false;
    }

    if (trade_currency == Currency::UNKNOWN) {
         LOG("ClientSession WARNING : Devise inconnue spécifiée dans la requête de trading pour " + clientId, "WARNING");
         if (client && client->isConnected()) client->send("ERROR: Unknown currency specified.\n");
         return false;
    }
     // On s'assure que la requête est pour SRD-BTC BUY/SELL via commandes client
     if (trade_currency != Currency::SRD_BTC || (req_type != RequestType::BUY && req_type != RequestType::SELL)) {
        LOG("ClientSession WARNING : Type de requête ou devise non supporté par handleClientTradeRequest pour client " + clientId, "WARNING");
        if (client && client->isConnected()) client->send("ERROR: Only BUY/SELL of SRD-BTC is supported via client command.\n");
        return false;
    }

    Currency balance_currency_for_percentage = (req_type == RequestType::BUY) ? Currency::USD : Currency::SRD_BTC;

    // Obtenir le solde actuel JUSTE pour calculer le MONTANT VOULU par le client.
    // Cette valeur SERA RE-VERIFIEE et ajustée dans la TQ au prix d'exécution.
    double current_balance_for_calc = clientWallet->getBalance(balance_currency_for_percentage);
    double amount_based_on_percentage = current_balance_for_calc * (percentage / 100.0);

     if (amount_based_on_percentage <= 0) {
        LOG("ClientSession WARNING : " + clientId + " - " + requestTypeToString(req_type) + " " + std::to_string(percentage) + "% " + currencyToString(trade_currency) + " : Montant calculé nul ou négatif (" + std::to_string(amount_based_on_percentage) + "). Solde " + currencyToString(balance_currency_for_percentage) + ": " + std::to_string(current_balance_for_calc), "WARNING");
        if (client && client->isConnected()) client->send("ERROR: Calculated amount is zero or negative. Check balance and percentage.\n");
        return false;
    }


    double crypto_quantity_requested = 0.0;

    if (req_type == RequestType::BUY) {
        // Pour un BUY, le client veut utiliser un montant en USD.
        // On calcule la quantité de crypto correspondante basée sur le prix ACTUEL (au moment de la commande, sera re-vérifié par TQ).
        double current_price_srd_btc = Global::getPrice(currencyToString(Currency::SRD_BTC));
        if (current_price_srd_btc <= 0 || !std::isfinite(current_price_srd_btc)) {
            LOG("ClientSession ERROR : Prix SRD-BTC non disponible ou invalide (" + std::to_string(current_price_srd_btc) + ") pour requête BUY de client " + clientId, "ERROR");
            if (client && client->isConnected()) client->send("ERROR: Current price not available for BUY.\n");
            return false;
        }
        crypto_quantity_requested = amount_based_on_percentage / current_price_srd_btc;
        LOG("ClientSession DEBUG : BUY calculé (basé sur prix actuel " + std::to_string(current_price_srd_btc) + "): utiliser " + std::to_string(amount_based_on_percentage) + " USD (" + std::to_string(percentage) + "%) pour une quantité visée de " + std::to_string(crypto_quantity_requested) + " SRD-BTC.", "DEBUG");

    } else if (req_type == RequestType::SELL) {
        // Pour un SELL, le client veut vendre un pourcentage de sa crypto.
        // amount_based_on_percentage est déjà la quantité de crypto visée.
        crypto_quantity_requested = amount_based_on_percentage;
        LOG("ClientSession DEBUG : SELL calculé : vendre " + std::to_string(crypto_quantity_requested) + " SRD-BTC (basé sur " + std::to_string(percentage) + "% de solde).", "DEBUG");
    }

   // Une quantité crypto nulle après calcul est une erreur.
   if (crypto_quantity_requested <= 0) {
        LOG("ClientSession WARNING : " + clientId + " - " + requestTypeToString(req_type) + " " + std::to_string(percentage) + "% " + currencyToString(trade_currency) + " : Quantité crypto calculée nulle ou négative (" + std::to_string(crypto_quantity_requested) + ").", "WARNING");
        if (client && client->isConnected()) client->send("ERROR: Calculated crypto quantity is zero or negative.\n");
        return false;
   }

    // Créer la requête de transaction.
    TransactionRequest request(
        clientId,
        req_type,
        currencyToString(trade_currency), // Utilise le nom de la crypto (ex: "SRD-BTC")
        crypto_quantity_requested         // La quantité (en crypto) calculée à trader
    );

    // Soumettre la requête à la file d'attente globale (TransactionQueue)
    extern TransactionQueue txQueue;
    txQueue.addRequest(request);

    LOG("ClientSession INFO : Requête de transaction soumise à la TQ par client " + clientId + ": Client=" + clientId + ", Type=" + requestTypeToString(req_type) + ", Qty Visée=" + std::to_string(crypto_quantity_requested) + " " + currencyToString(trade_currency) + " (basé sur " + std::to_string(percentage) + "% de solde " + currencyToString(balance_currency_for_percentage) + ")", "INFO");

    return true;
}


// --- Implémentation Corrigée de submitBotOrder ---
// Soumet un ordre décidé par le bot.
// Ne fait PLUS de vérification de fonds préliminaire ici.
// Calcule la quantité basée sur l'action du bot et soumet à la TQ.
bool ClientSession::submitBotOrder(TradingAction action) {
   if (!clientWallet) {
       LOG("ClientSession ERROR : Portefeuille non disponible pour bot " + clientId + " pour action " + std::to_string(static_cast<int>(action)), "ERROR");
       // Pas d'envoi au client ici, c'est le bot qui est concerné.
       return false;
   }

   RequestType req_type = RequestType::UNKNOWN_REQUEST;
   Currency trade_currency = Currency::SRD_BTC;

   double amount_to_trade_base = 0.0; // Montant dans la devise de base (USD pour BUY, SRD-BTC pour SELL)
   double percentage_used = 0.0;

   if (action == TradingAction::BUY) {
       req_type = RequestType::BUY;
       double current_usd_balance = clientWallet->getBalance(Currency::USD);
       // BOT_INVESTMENT_PERCENTAGE doit être défini (const double quelque part, ex: Global.h)
       percentage_used = BOT_INVESTMENT_PERCENTAGE;
       amount_to_trade_base = current_usd_balance * (percentage_used / 100.0);

       // === RETIRE la Vérification des fonds USD ici ! ===
       // La vérification finale et fiable sera faite par la TQ sous le verrou du Wallet.

   } else if (action == TradingAction::CLOSE_LONG) {
       req_type = RequestType::SELL;
       double current_srd_btc_balance = clientWallet->getBalance(Currency::SRD_BTC);
       amount_to_trade_base = current_srd_btc_balance; // Vendre tout
       percentage_used = 100.0;

        // === RETIRE la Vérification des fonds SRD-BTC ici ! ===
        // La vérification finale et fiable sera faite par la TQ sous le verrou du Wallet.

   } else {
       if (action != TradingAction::HOLD) { // HOLD est une action Bot valide qui ne soumet pas d'ordre
           LOG("ClientSession WARNING : Bot " + clientId + " a retourné une action non gérée pour soumission d'ordre : " + std::to_string(static_cast<int>(action)), "WARNING");
       }
       return false; // Aucune soumission pour UNKNOWN ou HOLD
   }

    // Un montant de base calculé nul ou négatif est une erreur.
   if (amount_to_trade_base <= 0) {
        LOG("ClientSession WARNING : " + clientId + " - " + requestTypeToString(req_type) + " bot : Montant base calculé nul ou négatif (" + std::to_string(amount_to_trade_base) + "). Action: " + std::to_string(static_cast<int>(action)) + ", Solde USD/SRD-BTC: " + std::to_string((req_type == RequestType::BUY) ? clientWallet->getBalance(Currency::USD) : clientWallet->getBalance(Currency::SRD_BTC)) + ".", "WARNING");
        return false;
   }


   double crypto_quantity_requested = 0.0;

   if (req_type == RequestType::BUY) {
       // Pour un BUY, le bot veut utiliser un montant en USD.
       // On calcule la quantité de crypto correspondante basée sur le prix ACTUEL (au moment de l'appel Bot, sera re-vérifié par TQ).
       double current_price_srd_btc = Global::getPrice(currencyToString(Currency::SRD_BTC));
        if (current_price_srd_btc <= 0 || !std::isfinite(current_price_srd_btc)) {
            LOG("ClientSession ERROR : Prix SRD-BTC non disponible ou invalide (" + std::to_string(current_price_srd_btc) + ") pour ordre BUY bot " + clientId, "ERROR");
            return false;
        }
       crypto_quantity_requested = amount_to_trade_base / current_price_srd_btc;
       LOG("ClientSession DEBUG : Bot BUY calculé (basé sur prix actuel " + std::to_string(current_price_srd_btc) + "): utiliser " + std::to_string(amount_to_trade_base) + " USD (" + std::to_string(percentage_used) + "%) pour une quantité visée de " + std::to_string(crypto_quantity_requested) + " SRD-BTC.", "DEBUG");

   } else if (req_type == RequestType::SELL) {
       // Pour un SELL, le bot veut vendre une quantité de sa crypto.
       // amount_to_trade_base est déjà la quantité de crypto visée (solde entier ou partiel).
       crypto_quantity_requested = amount_to_trade_base;
       LOG("ClientSession DEBUG : Bot SELL calculé : vendre " + std::to_string(crypto_quantity_requested) + " SRD-BTC (basé sur " + std::to_string(percentage_used) + "%).", "DEBUG");
   }

   // Une quantité crypto nulle après calcul est une erreur.
   if (crypto_quantity_requested <= 0) {
        LOG("ClientSession WARNING : " + clientId + " - " + requestTypeToString(req_type) + " bot : Quantité crypto calculée nulle ou négative après calcul. Montant base: " + std::to_string(amount_to_trade_base) + ", Qty crypto: " + std::to_string(crypto_quantity_requested) + ".", "WARNING");
        return false;
   }

   TransactionRequest request(
       clientId,
       req_type,
       currencyToString(trade_currency),
       crypto_quantity_requested
   );

   extern TransactionQueue txQueue;
   txQueue.addRequest(request);

   LOG("ClientSession INFO : Requête de transaction soumise à la TQ par bot " + clientId + ": Client=" + clientId + ", Type=" + requestTypeToString(req_type) + ", Qty Visée=" + std::to_string(crypto_quantity_requested) + " " + currencyToString(trade_currency) + " (basé sur " + std::to_string(percentage_used) + "%)", "INFO");

   return true;
}


// --- Implémentation Corrigée de applyTransactionRequest (Notification SEULEMENT) ---
// Appelé par le thread worker de la TransactionQueue.
// Reçoit l'objet Transaction final. Ne met PLUS à jour le Wallet ici.
void ClientSession::applyTransactionRequest(const Transaction& tx) {
   LOG("ClientSession INFO : Notification de TQ reçue pour transaction " + tx.getClientId() + ":" + tx.getId() + " avec statut " + transactionStatusToString(tx.getStatus()), "INFO");

   // Vérifier que la notification est bien pour CETTE session (déjà fait par la TQ, mais re-vérif sécurité)
   if (tx.getClientId() != this->clientId) {
       LOG("ClientSession ERROR : Notification de TQ reçue pour le mauvais client! Attendu=" + this->clientId + ", Reçu=" + tx.getClientId(), "ERROR");
       return; // Ignorer si ce n'est pas pour nous
   }

   // Notifier le bot s'il est actif (même en cas d'échec TX)
   if (bot) {
       // Assurez-vous que bot->notifyTransactionCompleted prend const Transaction&
       bot->notifyTransactionCompleted(tx);
       LOG("ClientSession DEBUG : Notification TX passée au bot " + clientId + " pour Tx " + tx.getId() + " (Statut: " + transactionStatusToString(tx.getStatus()) + ")", "DEBUG");
   } else {
        LOG("ClientSession DEBUG : Bot non actif pour client " + clientId + ". Notification TX " + tx.getId() + " non passée au bot.", "DEBUG");
   }

   // Formater et envoyer le message de résultat au client (si la connexion est toujours active)
   std::stringstream result_msg_ss;
   result_msg_ss << "TRANSACTION_RESULT ID=" << tx.getId()
                 << " STATUS=" << transactionStatusToString(tx.getStatus());

   if (tx.getStatus() == TransactionStatus::COMPLETED) {
        result_msg_ss << " TYPE=" << transactionTypeToString(tx.getType())
                      << " QTY=" << std::fixed << std::setprecision(8) << tx.getQuantity()
                      << " TOTAL=" << std::fixed << std::setprecision(2) << tx.getTotalAmount();
                      result_msg_ss << " PRICE=" << std::fixed << std::setprecision(8) << tx.getUnitPrice();
   } else if (tx.getStatus() == TransactionStatus::FAILED) {
       result_msg_ss << " REASON=" << tx.getFailureReason();
   }
   result_msg_ss << "\n";


   if (client && client->isConnected()) {
       try {
           client->send(result_msg_ss.str());
           LOG("ClientSession INFO : Résultat de TQ envoyé à client " + clientId + " pour Tx " + tx.getId() + " (" + transactionStatusToString(tx.getStatus()) + "). Message: '" + result_msg_ss.str() + "'", "INFO");

       } catch (const std::exception& e) {
            LOG("ClientSession ERROR : Erreur lors de l'envoi du résultat de TQ à " + clientId + ": " + e.what(), "ERROR");
       }
   } else {
        LOG("ClientSession WARNING : Impossible d'envoyer le résultat de TQ à client " + clientId + ", client déconnecté ou objet client invalide pour Tx " + tx.getId() + ".", "WARNING");
   }
}


// --- Démarre la logique du bot ---
void ClientSession::startBot(int bollingerPeriod, double bollingerK) {
   if (bot) {
       LOG("ClientSession WARNING : Bot déjà actif pour client " + clientId + ", ignorer START BOT.", "WARNING");
       if (client && client->isConnected()) client->send("ERROR: Bot is already running.\n");
       return;
   }

   // TODO : Le constructeur du Bot doit prendre un pointeur vers la ClientSession
   // pour pouvoir appeler submitBotOrder(). Utiliser weak_ptr ou shared_ptr.
   // Le Bot a besoin de clientWallet et aussi d'un moyen d'appeler submitBotOrder.
   // Il doit donc recevoir un pointeur vers la ClientSession (this).
   // Utilisez shared_from_this() ici pour obtenir un shared_ptr vers cette ClientSession.
   // Passez une weak_ptr au Bot si son cycle de vie est indépendant de la Session.
   // Pour l'instant, passons un shared_ptr (plus simple si Bot s'arrête avec Session).

   bot = std::make_shared<Bot>(clientId, bollingerPeriod, bollingerK, clientWallet); // TODO: Adapter constructeur Bot
   // TODO: Passer 'shared_from_this()' au constructeur du Bot si nécessaire
   // bot = std::make_shared<Bot>(clientId, bollingerPeriod, bollingerK, clientWallet, shared_from_this()); // Exemple


   if (bot) {
        // TODO: Appeler une méthode start sur l'objet Bot pour lancer son thread de logique.
        // bot->start(); // Exemple
        LOG("ClientSession INFO : Bot créé (thread non lancé si start() non appelé) pour client " + clientId, "INFO");
        if (client && client->isConnected()) client->send("BOT STARTED.\n");
   } else {
       LOG("ClientSession ERROR : Échec de création de l'objet Bot pour client " + clientId, "ERROR");
       if (client && client->isConnected()) client->send("ERROR: Failed to start bot.\n");
   }
}

// --- Arrête la logique du bot ---
void ClientSession::stopBot() {
   if (!bot) {
       LOG("ClientSession WARNING : Aucun bot actif pour client " + clientId + ", ignorer STOP BOT.", "WARNING");
        if (client && client->isConnected()) client->send("ERROR: No bot is running.\n");
       return;
   }

   // TODO : Appeler une méthode stop sur l'objet Bot avant de le détruire
   // pour que son thread puisse s'arrêter proprement.
   // bot->stop(); // Exemple

   bot = nullptr;
   LOG("ClientSession INFO : Bot arrêté pour client " + clientId, "INFO");
   if (client && client->isConnected()) client->send("BOT STOPPED.\n");
}


// --- Getters simples ---
const std::string& ClientSession::getId() const { return clientId; }
std::shared_ptr<Bot> ClientSession::getBot() const { return bot; }
std::shared_ptr<ServerConnection> ClientSession::getClientConnection() const { return client; }
std::shared_ptr<Wallet> ClientSession::getClientWallet() const { return clientWallet; }