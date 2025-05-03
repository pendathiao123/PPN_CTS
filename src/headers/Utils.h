#ifndef UTILS_H
#define UTILS_H

#include <string>       
#include <vector>
#include <stddef.h>     
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/err.h>


// Déclarations des fonctions utilitaires globales (implémentées dans Utils.cpp)

// Génère une chaîne d'octets aléatoires cryptographiquement sécurisée et la retourne en format hexadécimal.
// Utilise OpenSSL RAND_bytes. La chaîne hexadécimale sera de longueur 2*length.
// Param length: Longueur de la séquence d'octets aléatoires souhaitée.
// Retourne une chaîne hexadécimale, ou une chaîne vide en cas d'échec.
std::string GenerateRandomHex(size_t length);

// Génère un ID aléatoire simple (non cryptographiquement sûr).
// Utilise std::random.
// Retourne un ID numérique aléatoire sur une plage large.
std::string GenerateRandomId();

// Génère un token cryptographique sécurisé (ex: jeton de session) via HMAC-SHA256.
// Utilise des données aléatoires internes pour la clé et le message de l'HMAC.
// Retourne un token hexadécimal robuste, ou une chaîne vide en cas d'échec.
std::string GenerateToken();

// Hashe un mot de passe en clair de manière sécurisée (avec salage).
// Utilise des algorithmes de hachage de mot de passe sécurisés.
// Param password_plain: Le mot de passe en clair.
// Retourne le hash du mot de passe (incluant le sel), ou une chaîne vide en cas d'échec.
std::string HashPasswordSecure(const std::string& password_plain);

// Vérifie si un mot de passe en clair correspond à un hash stocké.
// Param password_plain: Le mot de passe en clair à vérifier.
// Param stored_hash: Le hash (incluant le sel) stocké.
// Retourne true si le mot de passe correspond au hash, false sinon.
bool VerifyPasswordSecure(const std::string& password_plain, const std::string& stored_hash);


// Déclaration de la callback de débuggage OpenSSL
void openssl_debug_callback(const SSL* ssl, int where, int ret);


#endif