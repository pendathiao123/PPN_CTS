#include <iostream>
#include "Server.h"
#include "Client.h"


int main() {
    Client client("127.0.0.1", 8080);
    client.sendRequest("GET /market");
    std::cout << "Réponse du serveur: " << client.receiveResponse() << std::endl;
    return 0;
}