#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <random>
#include <sstream>
#include <openssl/rand.h>

// Fonction pour générer une chaîne aléatoire
std::string GenerateRandomString(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    unsigned char rand_bytes[length];
    if (RAND_bytes(rand_bytes, length) != 1) {
        std::cerr << "Erreur de génération aléatoire" << std::endl;
        return "";
    }

    for (size_t i = 0; i < length; ++i) {
        result += charset[rand_bytes[i] % (sizeof(charset) - 1)];
    }

    return result;
}

// Fonction pour générer un ID à 4 chiffres aléatoires
std::string GenerateRandomId() {
    // Création d'un générateur de nombres aléatoires et d'une distribution pour 4 chiffres
    std::random_device rd; // Pour générer une valeur initiale aléatoire
    std::mt19937 gen(rd()); // Moteur de génération de nombres aléatoires
    std::uniform_int_distribution<> dis(1000, 9999); // Plage de 4 chiffres

    // Générer un nombre aléatoire entre 1000 et 9999
    int id = dis(gen);

    // Convertir le nombre en chaîne de caractères
    return std::to_string(id);
}

//Fonction GenerateToken utilisant HMAC
std::string GenerateToken() {
    // Générer une clé aléatoire de 32 octets
    std::string key = GenerateRandomString(32); // 32 caractères aléatoires
    // Générer un message aléatoire
    std::string message = GenerateRandomString(16); // 16 caractères pour le message

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    // Calculer le HMAC avec la clé et le message aléatoires
    HMAC(EVP_sha256(), key.c_str(), key.size(),
         reinterpret_cast<const unsigned char*>(message.c_str()), message.size(),
         hash, &hash_len);

    // Convertir le hash en une chaîne hexadécimale
    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    // Retourner la chaîne générée comme token
    return oss.str();
}

// Charger les utilisateurs depuis un fichier
std::unordered_map<std::string, std::string> LoadUsers(const std::string& filename) {
    std::unordered_map<std::string, std::string> users;
    std::ifstream file(filename);
    std::string id, token;
    while (file >> id >> token) {
        users[id] = token;
    }
    return users;
}

// Sauvegarder les utilisateurs dans un fichier
void SaveUsers(const std::string& filename, const std::unordered_map<std::string, std::string>& users) {
    std::ofstream file(filename, std::ios::trunc);
    for (const auto& [id, token] : users) {
        file << id << " " << token << "\n";
    }
}

// Initialiser le contexte SSL pour le serveur
SSL_CTX* InitServerCTX(const std::string& certFile, const std::string& keyFile) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// Accepter une connexion SSL
SSL* AcceptSSLConnection(SSL_CTX* ctx, int clientSocket) {
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return nullptr;
    }
    return ssl;
}

// Gérer une connexion client
void HandleClient(SSL* ssl, std::unordered_map<std::string, std::string>& users, const std::string& usersFile) {
    char buffer[1024] = {0};
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        return;
    }

    buffer[bytes] = '\0';
    
    // Convertir le buffer char* en std::string pour utiliser les méthodes de string
    std::string receivedMessage(buffer);
    std::cout << "Received: " << receivedMessage << std::endl;

    std::string response;

    // Extraire ID et token depuis la requête
    std::string id, token;
    std::string prefix_id = "ID:";
    std::string prefix_token = "TOKEN:";

    // Recherche de ID et TOKEN dans la chaîne reçue
    if (receivedMessage.find(prefix_id) != std::string::npos && receivedMessage.find(prefix_token) != std::string::npos) {
        // Extraire l'ID
        size_t id_start = receivedMessage.find(prefix_id) + prefix_id.length();
        size_t id_end = receivedMessage.find(",", id_start);
        id = receivedMessage.substr(id_start, id_end - id_start);

        // Extraire le TOKEN
        size_t token_start = receivedMessage.find(prefix_token) + prefix_token.length();
        token = receivedMessage.substr(token_start);

        std::cout << "Extracted ID: " << id << ", Token: " << token << std::endl;

        // Vérification des identifiants dans le fichier des utilisateurs
        auto it = users.find(id);
        if (it != users.end() && it->second == token) {
            // Authentification réussie
            response = "AUTH OK";
        } else {
            // Authentification échouée
            response = "AUTH FAIL";
        }
    } else {
        // Si le message n'est pas dans le format attendu, générer un nouvel ID et token
        // Générer un ID unique basé sur la taille de l'élément dans users
        id = GenerateRandomId();
        token = GenerateToken();  

        // Ajouter l'utilisateur à la liste des utilisateurs et sauvegarder
        users[id] = token;
        SaveUsers(usersFile, users);

        std::cout << "New ID: " << id << ", New Token: " << token << std::endl;

        // Envoyer au client les nouveaux identifiants
        response = "Identifiants: " + id + " " + token;
    }

    // Répondre au client
    SSL_write(ssl, response.c_str(), response.size());
}



// Fonction principale pour démarrer le serveur
void StartServer(int port, const std::string& certFile, const std::string& keyFile, const std::string& usersFile) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 5) < 0) {
        perror("Listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    SSL_CTX* ctx = InitServerCTX(certFile, keyFile);
    std::cout << "Server listening on port " << port << std::endl;

    // Charger les utilisateurs existants
    auto users = LoadUsers(usersFile);

    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        SSL* ssl = AcceptSSLConnection(ctx, clientSocket);
        if (ssl) {
            HandleClient(ssl, users, usersFile);
            SSL_free(ssl);
        }
        close(clientSocket);
    }

    close(serverSocket);
    SSL_CTX_free(ctx);
}

int main() {
    StartServer(4433, "server.crt", "server.key", "configFile.csv");
    return 0;
}
