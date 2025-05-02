// src/headers/Utils.h - Fonctions utilitaires globales

#ifndef UTILS_H
#define UTILS_H

#include <string>       // Pour std::string
#include <vector>       // Pour std::vector (si utilisé par GenerateRandomString par ex)
#include <stddef.h>     // For size_t
#include <openssl/rand.h> // RAND_bytes (si GenerateRandomString l'utilise directement)
#include <openssl/hmac.h> // HMAC (si GenerateToken l'utilise directement)
#include <openssl/evp.h>  // EVP_sha256 (si GenerateToken l'utilise directement)
#include <openssl/err.h>  // ERR_print_errors_fp (si utile dans les utilitaires)


// Déclarations des fonctions utilitaires globales (implémentées dans Utils.cpp)

/**
 * @brief Génère une chaîne d'octets aléatoires cryptographiquement sécurisée et la retourne en format hexadécimal.
 * Utilise OpenSSL RAND_bytes.
 * @param length Longueur de la séquence d'octets aléatoires souhaitée (la chaîne hexadécimale sera de longueur 2*length).
 * @return Une chaîne hexadécimale représentant les octets aléatoires, ou une chaîne vide en cas d'échec.
 */
std::string GenerateRandomHex(size_t length);

/**
 * @brief Génère un ID aléatoire simple (non cryptographiquement sûr).
 * Utilise std::random.
 * @return Une chaîne représentant un ID numérique aléatoire sur une plage large.
 */
std::string GenerateRandomId();

/**
 * @brief Génère un token cryptographique sécurisé (ex: jeton de session) via HMAC-SHA256.
 * Utilise des données aléatoires internes pour la clé et le message de l'HMAC.
 * @return Un token hexadécimal robuste, ou une chaîne vide en cas d'échec.
 */
std::string GenerateToken();

// --- Placeholders pour fonctions de sécurité des mots de passe (à implémenter) ---
// Ces fonctions DOIVENT utiliser des algorithmes de hachage de mot de passe sécurisés (bcrypt, Argon2, scrypt).
// Elles seront implémentées dans Utils.cpp ou un fichier de sécurité dédié.

/**
 * @brief Hashe un mot de passe en clair de manière sécurisée (avec salage).
 * @param password_plain Le mot de passe en clair.
 * @return Le hash du mot de passe (incluant le sel), ou une chaîne vide en cas d'échec.
 */
std::string HashPasswordSecure(const std::string& password_plain);

/**
 * @brief Vérifie si un mot de passe en clair correspond à un hash stocké.
 * @param password_plain Le mot de passe en clair à vérifier.
 * @param stored_hash Le hash (incluant le sel) stocké.
 * @return true si le mot de passe correspond au hash, false sinon.
 */
bool VerifyPasswordSecure(const std::string& password_plain, const std::string& stored_hash);


// Déclaration de la callback de débuggage OpenSSL
void openssl_debug_callback(const SSL* ssl, int where, int ret); // <-- Ajoutez cette ligne



#endif // UTILS_H