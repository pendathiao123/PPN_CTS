#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

const int PORT = 8080;
const int BUFFER_SIZE = 1024;

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // 1. Créer le socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erreur lors de la création du socket");
        return 1;
    }

    // 2. Configurer l'adresse du serveur
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 3. Convertir l'adresse IP en format binaire
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Adresse invalide ou non supportée");
        return 1;
    }

    // 4. Se connecter au serveur
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erreur de connexion");
        return 1;
    }

    // 5. Envoyer un message au serveur
    const char *message = "Bonjour, serveur!";
    send(sock, message, strlen(message), 0);
    std::cout << "Message envoyé au serveur\n";

    // 6. Recevoir une réponse du serveur
    read(sock, buffer, BUFFER_SIZE);
    std::cout << "Réponse du serveur : " << buffer << std::endl;

    // 7. Fermer le socket
    close(sock);
    return 0;
}
