// --- Implémentation des fonctions utilitaires globales ---

#include "../headers/Utils.h" // Inclut les déclarations
#include "../headers/Logger.h" // Pour la macro LOG

#include <openssl/rand.h>      // RAND_bytes
#include <openssl/hmac.h>      // HMAC
#include <openssl/evp.h>       // EVP_sha256, EVP_MAX_MD_SIZE
#include <openssl/err.h>       // ERR_print_errors_fp
#include <openssl/ssl.h>
#include <iostream>            // std::cerr
#include <sstream>             // std::ostringstream
#include <iomanip>             // std::setw, std::setfill
#include <random>              // std::random_device, std::mt19937, std::uniform_int_distribution
#include <vector>              // std::vector
#include <string>              // std::string
#include <stddef.h>            // size_t


// Implémentation des fonctions utilitaires globales déclarées dans Utils.h.

// Génère une chaîne d'octets aléatoires cryptographiquement sécurisée et la retourne en format hexadécimal.
std::string GenerateRandomHex(size_t length) {
    std::vector<unsigned char> rand_bytes(length);

    if (RAND_bytes(rand_bytes.data(), length) != 1) {
        LOG("GenerateRandomHex ERROR : Erreur de génération aléatoire RAND_bytes.", "ERROR");
        ERR_print_errors_fp(stderr); // Log OpenSSL error
        return "";
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        oss << std::setw(2) << (int)rand_bytes[i];
    }
    return oss.str(); // Retourne une chaîne hexadécimale.
}

// Génère un ID aléatoire simple (non cryptographiquement sûr).
std::string GenerateRandomId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    // Plage large pour l'ID numérique.
    std::uniform_int_distribution<> dis(100000000, 999999999);

    return std::to_string(dis(gen));
}

// Génère un token cryptographique sécurisé (ex: jeton de session) via HMAC-SHA256.
// Utilise des données aléatoires internes pour la clé et le message de l'HMAC.
std::string GenerateToken() {
    // Génère une clé aléatoire robuste (256 bits pour SHA256).
    std::string key_hex = GenerateRandomHex(32);
    // Génère un message aléatoire (assez long).
    std::string message_hex = GenerateRandomHex(16);

    // Vérifie si la génération aléatoire a réussi.
    if (key_hex.empty() || message_hex.empty()) {
         LOG("GenerateToken ERROR : Impossible de générer le token car clé ou message aléatoire est vide.", "ERROR");
         return "";
    }

    // NOTE : HMAC prend des pointeurs vers les octets bruts. GenerateRandomHex retourne une string hexadécimale.
    // Il faudrait convertir les strings hexadécimales key_hex et message_hex en vecteurs d'octets bruts.
    // Simplifions en utilisant des octets aléatoires directement pour la clé et le message de l'HMAC si possible.
    // Ou appelons GenerateRandomHex avec une longueur appropriée et convertissons.

    // Option 1 : Utiliser GenerateRandomHex et reconvertir en binaire (complexe).
    // Option 2 : Refaire l'appel à RAND_bytes ici pour les données binaires.
    // Option 2 est plus simple si GenerateRandomHex est juste pour l'ID visible.

    std::vector<unsigned char> hmac_key(32); // Clé de 32 octets pour HMAC-SHA256
    std::vector<unsigned char> hmac_message(16); // Message de 16 octets

    if (RAND_bytes(hmac_key.data(), hmac_key.size()) != 1 || RAND_bytes(hmac_message.data(), hmac_message.size()) != 1) {
        LOG("GenerateToken ERROR : Erreur de génération aléatoire pour les données HMAC.", "ERROR");
        ERR_print_errors_fp(stderr);
        return "";
    }


    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    // Calcule le HMAC-SHA256.
    if (HMAC(EVP_sha256(),
             hmac_key.data(), hmac_key.size(), // Clé binaire
             hmac_message.data(), hmac_message.size(), // Message binaire
             hash, &hash_len) == nullptr) // Buffer de sortie et longueur
    {
        LOG("GenerateToken ERROR : Erreur lors du calcul HMAC.", "ERROR");
        ERR_print_errors_fp(stderr);
        return "";
    }

    // Convertit le hash binaire résultant en chaîne hexadécimale pour le token.
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::setw(2) << (int)hash[i];
    }

    return oss.str(); // Retourne le token sous forme de string hexadécimale.
}

// Implémentation de la callback de débuggage OpenSSL
void openssl_debug_callback(const SSL* ssl, int where, int ret) {
    const char *str;
    int w = where & SSL_ST_MASK;
    if (w == SSL_ST_CONNECT) str = "SSL_connect";
    else if (w == SSL_ST_ACCEPT) str = "SSL_accept";
    else str = "undefined";

    if (where & SSL_CB_LOOP) {
        LOG("OpenSSL Callback: " + std::string(str) + ":" + std::string(SSL_state_string_long(ssl)), "DEBUG");
    } else if (where & SSL_CB_ALERT) {
        str = (where & SSL_CB_READ) ? "read" : "write";
        LOG("OpenSSL Callback: SSL3 alert " + std::string(str) + ":" + SSL_alert_type_string_long(ret) + ":" + SSL_alert_desc_string_long(ret), "DEBUG");
    } else if (where & SSL_CB_EXIT) {
         LOG("OpenSSL Callback: SSL_CB_ERROR - " + std::string(SSL_state_string_long(ssl)), "ERROR");
    } else if (where & (SSL_CB_HANDSHAKE_START | SSL_CB_HANDSHAKE_DONE)) {
         LOG("OpenSSL Callback: Handshake " + std::string(SSL_state_string_long(ssl)) + ((where & SSL_CB_HANDSHAKE_DONE) ? " DONE" : " START"), "DEBUG");
    }
}


// --- Placeholders pour fonctions de sécurité des mots de passe (à implémenter) ---
// Ces fonctions DOIVENT utiliser des algorithmes de hachage de mot de passe sécurisés (bcrypt, Argon2, scrypt).
// L'implémentation réelle nécessite l'intégration d'une bibliothèque cryptographique appropriée.

std::string HashPasswordSecure(const std::string& password_plain) {
    LOG("SecurityUtils WARNING : HashPasswordSecure est un placeholder. REMPLACER PAR UN VRAI HACHAGE SECURISÉ (bcrypt, Argon2) !", "WARNING");
    // --- IMPLÉMENTATION PLACEHOLDER INSECURE ---
    // Pour l'instant, retourne juste le mot de passe + un suffixe pour distinguer.
    // CE N'EST PAS SÉCURISÉ !
    if (password_plain.empty()) return "";
    return password_plain + "_hashed_INSECURE";
    // --- FIN IMPLÉMENTATION PLACEHOLDER INSECURE ---
}

// Placeholder - REMPLACER PAR UNE VRAIE IMPLÉMENTATION DE VÉRIFICATION SÉCURISÉE !
bool VerifyPasswordSecure(const std::string& password_plain, const std::string& stored_hash) {
    LOG("SecurityUtils WARNING : VerifyPasswordSecure est un placeholder. REMPLACER PAR UNE VRAIE VÉRIFICATION DE HASH SÉCURISÉ !", "WARNING");
     // --- IMPLÉMENTATION PLACEHOLDER INSECURE ---
     // Pour l'instant, compare le mot de passe haché de manière PLACEHOLDER avec le hash stocké.
     // CE N'EST PAS SÉCURISÉ !
     if (password_plain.empty() || stored_hash.empty()) return false;
     return (password_plain + "_hashed_INSECURE") == stored_hash;
     // --- FIN IMPLÉMENTATION PLACEHOLDER INSECURE ---
}

// TODO: Implémenter HashPasswordSecure et VerifyPasswordSecure de manière sécurisée ici ou dans un autre fichier de sécurité.
// Ceci nécessite d'intégrer une bibliothèque comme libsodium, OpenSSL (avec des fonctions comme PBKDF2+HMAC), ou bcrypt/Argon2 directement.