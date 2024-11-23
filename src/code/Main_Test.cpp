#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"

int main() {
    // Initialisation du serveur avec l'adresse, le port et le fichier de configuration
    Server server("127.0.0.1", 8080, "crypto_config.txt");

    // Initialisation du client avec l'adresse du serveur et le port
    Client client("127.0.0.1", 8080);

    std::cout << "Server and Client are initialized and ready to communicate!" << std::endl;

    return 0;
}
