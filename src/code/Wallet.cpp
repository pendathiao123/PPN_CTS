// Implémentation de la classe Wallet

#include "../headers/Wallet.h"
#include "../headers/Logger.h"
#include "../headers/Transaction.h" // Pour Transaction, enums Currency/TransactionType/TransactionStatus, et helpers stringTo/ToString

#include <iostream>
#include <stdexcept>
#include <limits>       // numeric_limits
#include <fstream>      // ifstream, ofstream
#include <sstream>      // stringstream
#include <iomanip>      // fixed, setprecision
#include <chrono>       // chrono, system_clock
#include <ctime>        // time_t, localtime_r
#include <filesystem>   // filesystem::path, create_directories, error_code
#include <vector>
#include <cmath>        // abs, isfinite
#include <mutex>        // mutex, lock_guard
#include <cerrno>       // errno
#include <cstring>      // strerror


// --- Implémentation generateWalletFilePath ---
// Construit le chemin complet du fichier portefeuille. dataDirPath est le répertoire où stocker (e.g., "../src/data/wallets").
std::string Wallet::generateWalletFilePath(const std::string& dataDirPath) const {
    std::filesystem::path wallets_dir_path_obj = dataDirPath;
    std::filesystem::path wallet_file_name = clientId + ".wallet";

    std::filesystem::path wallet_file_path = wallets_dir_path_obj / wallet_file_name;

    return wallet_file_path.string();
}

// Dans src/code/Wallet.cpp

// --- Implémentation ensureWalletsDirectoryExists (Version Modifiée) ---
// S'assure que le répertoire du portefeuille existe.
bool Wallet::ensureWalletsDirectoryExists() const {
    std::filesystem::path file_path_obj = walletFilePath;
    std::filesystem::path wallets_dir_path = file_path_obj.parent_path();

    LOG("Wallet::ensureWalletsDirectoryExists DEBUG: Chemin répertoire à vérifier/créer : " + wallets_dir_path.string(), "DEBUG");

    std::error_code ec;
    bool result_create_dir = false; // Initialiser à false

    // Tenter la création de répertoire. Capturer les exceptions potentielles si elles ne sont pas mises dans ec.
    // std::filesystem::create_directories retourne true si le répertoire existait déjà ou a été créé avec succès.
    // Elle retourne false en cas d'échec, et devrait remplir ec.
    try {
         result_create_dir = std::filesystem::create_directories(wallets_dir_path, ec);
    } catch (const std::filesystem::filesystem_error& ex) {
         LOG("Wallet::ensureWalletsDirectoryExists ERROR: Exception filesystem lors de create_directories pour '" + wallets_dir_path.string() + "'. Erreur: " + ex.what(), "ERROR");
         return false; // Échec si une exception est levée.
    } catch (const std::exception& e) {
         // Attrape d'autres exceptions standards non liées à filesystem spécifiquement.
         LOG("Wallet::ensureWalletsDirectoryExists ERROR: Exception std lors de create_directories pour '" + wallets_dir_path.string() + "'. Erreur: " + e.what(), "ERROR");
         return false; // Échec si une exception est levée.
    }


    // Log détaillé APRES l'appel à create_directories pour comprendre le comportement.
    LOG("Wallet::ensureWalletsDirectoryExists DEBUG: create_directories retour : " + std::to_string(result_create_dir) + ", ec.value : " + std::to_string(ec.value()) + ", ec.message : '" + ec.message() + "', errno : " + std::to_string(errno) + ", strerror(errno) : '" + std::string(strerror(errno)) + "'", "DEBUG");


    if (result_create_dir) {
        // create_directories a retourné true. C'est le cas nominal (répertoire créé ou existait déjà).
        LOG("Wallet::ensureWalletsDirectoryExists DEBUG: Répertoire '" + wallets_dir_path.string() + "' créé ou existait et create_directories a retourné true.", "DEBUG");
        return true; // Succès confirmé.
    } else { // create_directories a retourné false.
         // Vérifier si error_code explique l'échec (permissions, chemin invalide, etc.).
         if (ec) { // Si error_code est défini, c'est la raison de l'échec.
             LOG("Wallet::ensureWalletsDirectoryExists ERROR: create_directories retourné false. Impossible de créer/vérifier répertoire '" + wallets_dir_path.string() + "'. Raison (ec): " + ec.message() + ".", "ERROR");
             return false; // Échec avec raison claire.
         } else {
             // create_directories retourné false MAIS error_code est vide/succès (ec.value() == 0 && ec.message() == "Success").
             // C'est le cas bizarre que vous avez observé.
             LOG("Wallet::ensureWalletsDirectoryExists WARNING: create_directories retourné false MAIS ec vide. Comportement inhabituel. Chemin: '" + wallets_dir_path.string() + "'.", "WARNING");

             // Dans ce cas bizarre, Tenter de vérifier manuellement si le répertoire existe MAINTENANT.
             // Si le répertoire existe après ce comportement bizarre, on le considère comme utilisable.
             std::error_code ec_check; // Nouvelle ec pour la vérification exists()
             bool exists_after = std::filesystem::exists(wallets_dir_path, ec_check);

             LOG("Wallet::ensureWalletsDirectoryExists DEBUG: Vérification manuelle après échec create_directories: exists() retour " + std::to_string(exists_after) + ", ec_check.message : '" + ec_check.message() + "'", "DEBUG");

             if (exists_after) {
                 // Le répertoire existe malgré l'échec bizarre de create_directories. On peut continuer.
                 LOG("Wallet::ensureWalletsDirectoryExists INFO: Répertoire '" + wallets_dir_path.string() + "' existe bien malgré l'échec bizarre de create_directories. Considéré comme succès pour la suite.", "INFO");
                 return true; // Le répertoire est là et utilisable, on retourne true.
             } else {
                  // Le répertoire n'existe toujours pas, même après l'appel bizarre. C'est un échec réel.
                  LOG("Wallet::ensureWalletsDirectoryExists ERROR: Répertoire '" + wallets_dir_path.string() + "' n'existe toujours pas après l'échec bizarre. Raison vérif manuelle (ec_check): " + ec_check.message() + ". Considéré comme échec.", "ERROR");
                  return false; // Le répertoire n'est pas là, on retourne false.
             }
         }
    }
}

// --- Implémentation du Constructeur ---
// dataDirPath DOIT être le chemin du répertoire des wallets (e.g., "../src/data/wallets").
Wallet::Wallet(const std::string& clientId, const std::string& dataDirPath)
    // Initialise les membres dans l'ordre de déclaration
    : clientId(clientId),
      dataDirectoryPath(dataDirPath), // dataDirPath EST le chemin du répertoire wallets (selon analyse)
      walletFilePath(generateWalletFilePath(dataDirPath)) // CORRECT : Utilise dataDirPath tel quel
{
    // Initialise les soldes par défaut si le fichier ne contient pas ces devises.
    // loadFromFile va écraser si elles sont présentes dans le fichier.
    balances[Currency::USD] = 0.0;
    balances[Currency::SRD_BTC] = 0.0;

    // Tente de charger depuis le fichier.
    if (loadFromFile()) {
        LOG("Wallet Portefeuille chargé avec succès pour client ID: " + clientId + " depuis " + walletFilePath, "INFO");
    } else {
        // loadFromFile loggue déjà la raison.
        LOG("Wallet Impossible de charger le portefeuille pour client ID: " + clientId + ". Un nouveau portefeuille vierge sera utilisé. Chemin cherché : " + walletFilePath, "WARNING");
    }
    LOG("Wallet Objet Wallet créé pour client ID: " + clientId + ". Chemin du fichier: " + walletFilePath, "DEBUG");
}

// --- Implémentation du Destructeur ---
Wallet::~Wallet() {
     LOG("Wallet Objet Wallet détruit pour client ID: " + clientId + ". Sauvegarde finale en cours...", "DEBUG");
    if (saveToFile()) {
        LOG("Wallet Portefeuille pour client ID: " + clientId + " sauvegardé avec succès vers " + walletFilePath + " avant destruction.", "INFO");
    } else {
        LOG("Wallet Échec de la sauvegarde finale du portefeuille pour client ID: " + clientId + " vers " + walletFilePath + ".", "ERROR");
    }
     LOG("Wallet Objet Wallet détruit pour client ID: " + clientId + ". Sauvegarde finale terminée.", "DEBUG");
}

// --- Implémentation des méthodes de solde (Thread-Safe) ---

double Wallet::getBalance(Currency currency) const {
    std::lock_guard<std::mutex> lock(walletMutex); // Protège l'accès concurrent en lecture
    auto it = balances.find(currency);
    if (it != balances.end()) {
        return it->second;
    }
    LOG("Wallet Portefeuille (" + clientId + ") : Demande de solde pour devise inconnue : " + currencyToString(currency), "ERROR");
    return 0.0;
}

// --- Implémentation des méthodes d'historique (Thread-Safe) ---

void Wallet::addTransaction(const Transaction& tx) {
    std::lock_guard<std::mutex> lock(walletMutex); // Protège l'accès concurrent
    if (tx.getClientId() != this->clientId) {
        LOG("Wallet Portefeuille (" + clientId + ") : Tentative d'ajouter transaction avec ClientId non correspondant ('" + tx.getClientId() + "' vs '" + this->clientId + "'). Transaction ID: " + tx.getId() + ". Ignorée.", "ERROR");
        return;
    }
    transactionHistory.push_back(tx);
    LOG("Wallet Portefeuille (" + clientId + ") : Transaction ajoutée à l'historique. ID: " + tx.getId() + ", Type: " + transactionTypeToString(tx.getType()) + ", Statut: " + transactionStatusToString(tx.getStatus()), "INFO");
}

std::vector<Transaction> Wallet::getTransactionHistory() const {
    std::lock_guard<std::mutex> lock(walletMutex); // Protège l'accès concurrent en lecture
    return transactionHistory; // Retourne une copie (thread-safe)
}

// --- Implémentation des méthodes de persistance (Thread-Safe) ---

bool Wallet::loadFromFile() {
    std::lock_guard<std::mutex> lock(walletMutex); // Protège l'accès concurrent pendant le chargement

    if (!ensureWalletsDirectoryExists()) {
         // ensureWalletsDirectoryExists loggue déjà l'erreur
         return false;
    }

    std::ifstream file(walletFilePath);
    if (!file.is_open()) {
        LOG("Wallet Fichier portefeuille non trouvé ou impossible à ouvrir pour lecture : " + walletFilePath, "INFO");
        // Laisse les soldes et l'historique aux valeurs par défaut (0.0, vide)
        return false;
    }

    LOG("Wallet Lecture du fichier portefeuille : " + walletFilePath, "DEBUG");

    // Efface les données actuelles avant de charger
    balances.clear();
    transactionHistory.clear();

    // Ré-initialise les soldes par défaut au cas où le fichier soit vide ou mal formaté
    balances[Currency::USD] = 0.0;
    balances[Currency::SRD_BTC] = 0.0;


    std::string line;
    int balances_read_count = 0;

    // Lecture des soldes attendus en début de fichier
    while(std::getline(file, line) && balances_read_count < 2) { // Limite la boucle aux 2 premières lignes pour les soldes
        std::stringstream ss(line);
        std::string currency_str;
        double balance_val;
        if (ss >> currency_str >> balance_val) {
             Currency c = stringToCurrency(currency_str); // Utilise la fonction utilitaire
             if (c != Currency::UNKNOWN) {
                 // Applique la valeur lue, avec check de précision pour les très petits négatifs
                 balances[c] = (balance_val < 0 && std::abs(balance_val) < std::numeric_limits<double>::epsilon()) ? 0.0 : balance_val;
                 balances_read_count++;
             } else {
                 LOG("Wallet Portefeuille (" + clientId + "): Chargement - Devise inconnue '" + currency_str + "' rencontrée. Ligne: '" + line + "'. Ignorée.", "WARNING");
             }
        } else {
             // Ligne mal formatée, n'est pas un solde attendu. Potentiellement le début des transactions.
             // Remet la ligne dans le flux et sort de la boucle des soldes.
             // Attention : nécessite que le parsing des soldes soit strictement au début.
             file.seekg(-(line.length() + 1), std::ios_base::cur); // Déplace le curseur avant la ligne lue
             break;
        }
         // La condition de la boucle while(std::getline(file, line) && balances_read_count < 2) gère déjà la sortie après 2 soldes.
    }

    if (balances_read_count < 2) {
        LOG("Wallet Portefeuille (" + clientId + "): Chargement - Seulement " + std::to_string(balances_read_count) + " soldes trouvés dans le fichier (attendu 2 : USD, SRD-BTC). Les soldes manquants resteront à 0.0 ou leur valeur par défaut.", "INFO");
    }

    // Lecture de l'historique des transactions
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string marker;
        ss >> marker;
        if (marker == "TRANSACTION") {
            std::string id_str, clientId_str, type_str, cryptoName_str, status_str;
            double quantity_val, unitPrice_val, totalAmount_val, fee_val;
            long long timestamp_epoch; // Stocké en long long

            // Parsing des champs de la transaction
            if (!(ss >> id_str >> clientId_str >> type_str >> cryptoName_str >> quantity_val >> unitPrice_val >> totalAmount_val >> fee_val >> timestamp_epoch >> status_str)) {
                LOG("Wallet Portefeuille (" + clientId + "): Chargement - Erreur format ligne TRANSACTION : '" + line + "'. Champs manquants ou invalides. Ignorée.", "ERROR");
                continue; // Passe à la ligne suivante
            }

            // Validation que la transaction appartient bien à ce client
            if (clientId_str != this->clientId) {
                LOG("Wallet Portefeuille (" + clientId + "): Chargement - Transaction dans fichier avec ClientId non correspondant ('" + clientId_str + "' vs '" + this->clientId + "'). ID transaction: " + id_str + ". Ligne: '" + line + "'. Ignorée.", "WARNING");
                continue; // Passe à la ligne suivante
            }

            // Conversion des strings en enums
            TransactionType type_enum = stringToTransactionType(type_str); // Utilise la fonction utilitaire
            TransactionStatus status_enum = stringToTransactionStatus(status_str); // Utilise la fonction utilitaire

             if (type_enum == TransactionType::UNKNOWN) {
                 LOG("Wallet Portefeuille (" + clientId + "): Chargement - Transaction type UNKNOWN. ID: " + id_str + ", Type string: '" + type_str + "'.", "WARNING");
             }
             if (status_enum == TransactionStatus::UNKNOWN) {
                  LOG("Wallet Portefeuille (" + clientId + ") : Chargement - Transaction statut UNKNOWN. ID: " + id_str + ", Statut string: '" + status_str + "'.", "WARNING");
             }

             // Gérer le cas d'une transaction PENDING trouvée au chargement (serveur a crashé)
             if (status_enum == TransactionStatus::PENDING) {
                 LOG("Wallet Portefeuille (" + clientId + ") : Chargement - Transaction PENDING trouvée. ID: " + id_str + ". Statut changé à FAILED.", "WARNING");
                 status_enum = TransactionStatus::FAILED; // La marquer comme échouée
             }

            // Conversion du timestamp epoch en std::time_t
            std::time_t loaded_time_t = static_cast<std::time_t>(timestamp_epoch);

            // Création de l'objet Transaction avec les données chargées
            Transaction loaded_tx(id_str, clientId_str, type_enum, cryptoName_str, quantity_val, unitPrice_val, totalAmount_val, fee_val, loaded_time_t, status_enum);

            // Ajout à l'historique en mémoire
            transactionHistory.push_back(loaded_tx);

        } else {
             // Ligne qui n'est pas un marqueur TRANSACTION et n'est pas vide/commentaire (#)
             if (!line.empty() && line[0] != '#') { // Considère les lignes commençant par # comme commentaires
                 LOG("Wallet Portefeuille (" + clientId + ") : Chargement - Ligne inattendue ou mal formatée. Ligne: '" + line + "'. Ignorée.", "WARNING");
             }
        }
    }

    file.close(); // Ferme le fichier

    // Correction LOG + formatage final
    std::stringstream ss_final_log;
    ss_final_log << "Wallet Portefeuille (" << clientId << ") chargé avec succès : USD=" << std::fixed << std::setprecision(10) << balances[Currency::USD] << ", SRD-BTC=" << std::fixed << std::setprecision(10) << balances[Currency::SRD_BTC] << ", Transactions=" << transactionHistory.size();
    LOG(ss_final_log.str(), "INFO");
    return true; // Chargement réussi (même si le fichier était vide ou avec quelques lignes ignorées)
}

bool Wallet::saveToFile() const {
    std::lock_guard<std::mutex> lock(walletMutex); // Protège l'accès concurrent pendant la sauvegarde

    if (!ensureWalletsDirectoryExists()) {
         // ensureWalletsDirectoryExists loggue déjà l'erreur
         return false;
     }

    std::ofstream file(walletFilePath, std::ios::trunc); // Ouvre (ou crée) et vide le fichier
    if (!file.is_open()) {
        LOG("Wallet Erreur: Impossible d'ouvrir fichier portefeuille pour sauvegarde: " + walletFilePath + ". Erreur système: " + std::string(strerror(errno)), "ERROR");
        return false;
    }

    LOG("Wallet Sauvegarde du portefeuille : " + walletFilePath, "DEBUG");

    // Sauvegarde des soldes
    file << currencyToString(Currency::USD) << " " << std::fixed << std::setprecision(10) << balances.at(Currency::USD) << "\n"; // Utilise .at() pour un accès sécurisé (lève exception si la devise n'existe pas, ce qui ne devrait pas arriver)
    file << currencyToString(Currency::SRD_BTC) << " " << std::fixed << std::setprecision(10) << balances.at(Currency::SRD_BTC) << "\n";

    // Sauvegarde de l'historique des transactions
    for (const auto& tx : transactionHistory) {
        // Conversion du timestamp time_point en epoch (secondes) pour la sauvegarde
        long long timestamp_epoch = std::chrono::duration_cast<std::chrono::seconds>(tx.getTimestamp().time_since_epoch()).count();

        file << "TRANSACTION " // Marqueur pour identifier une ligne de transaction
             << tx.getId() << " "
             << tx.getClientId() << " "
             << transactionTypeToString(tx.getType()) << " " // Utilise la fonction utilitaire
             << tx.getCryptoName() << " "
             << std::fixed << std::setprecision(10) << tx.getQuantity() << " "
             << std::fixed << std::setprecision(10) << tx.getUnitPrice() << " "
             << std::fixed << std::setprecision(10) << tx.getTotalAmount() << " "
             << std::fixed << std::setprecision(10) << tx.getFee() << " "
             << timestamp_epoch << " "
             << transactionStatusToString(tx.getStatus()) // Utilise la fonction utilitaire
             << "\n";
    }

    file.close(); // Ferme le fichier

    // Vérifie si des erreurs d'écriture se sont produites avant ou pendant la fermeture
    if (file.fail()) {
         LOG("Wallet Erreur: Échec opération écriture/fermeture fichier portefeuille: " + walletFilePath + ". Erreur système: " + std::string(strerror(errno)), "ERROR");
         return false;
    }

    // Correction LOG + formatage final
    std::stringstream ss_final_log;
    ss_final_log << "Wallet Portefeuille (" << clientId << ") sauvegardé : USD=" << std::fixed << std::setprecision(10) << balances.at(Currency::USD) << ", SRD-BTC=" << std::fixed << std::setprecision(10) << balances.at(Currency::SRD_BTC) << ".";
    LOG(ss_final_log.str(), "INFO");

    return true; // Sauvegarde réussie
}

// --- Implémentation de updateBalance ---
// Met à jour le solde pour une devise donnée avec un montant donné.
// Le montant peut être positif (crédit) ou négatif (débit).
// DOIT être thread-safe en utilisant walletMutex en interne.
void Wallet::updateBalance(Currency currency, double amount) {
    // Le verrouillage est ESSENTIEL car cette méthode modifie les soldes.
    // C'est le verrouillage INTERNE du Wallet.
    std::lock_guard<std::mutex> lock(walletMutex);

    // Assurez-vous que la devise existe dans la map (le constructeur devrait déjà l'avoir fait pour USD/SRD-BTC)
    // Le constructeur initialise déjà USD et SRD-BTC à 0.0, donc at() est safe.
    // Utiliser at() pour s'assurer que la clé existe ou lancer une exception si ce n'est pas le cas (robustesse).
    try {
        balances.at(currency) += amount;
        LOG("Wallet DEBUG : Client " + clientId + " balance " + currencyToString(currency) + " mise à jour de " + std::to_string(amount) + ". Nouveau solde: " + std::to_string(balances.at(currency)), "DEBUG");
    } catch (const std::out_of_range& oor) {
         // Cette exception ne devrait pas arriver pour USD/SRD-BTC si le constructeur est bon.
         // Elle pourrait arriver si updateBalance est appelée avec une devise qui n'est pas gérée par le Wallet.
         LOG("Wallet ERROR : Client " + clientId + " updateBalance appelé avec devise inconnue : " + currencyToString(currency) + ". Montant : " + std::to_string(amount) + ". Erreur: " + oor.what(), "ERROR");
    }
}


// --- Implémentation getClientId ---
const std::string& Wallet::getClientId() const {
    return clientId;
}

// --- Implémentation de getMutex ---
// Utilisée par la TransactionQueue pour verrouiller ce Wallet avant de le modifier.
std::mutex& Wallet::getMutex() {
    // Pas de verrouillage ici, car on retourne le mutex lui-même.
    // C'est l'appelant (la TQ) qui utilisera cette référence pour créer un lock_guard.
    return walletMutex;
}