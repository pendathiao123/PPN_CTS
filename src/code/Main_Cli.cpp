#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"


int main() {
    Client client;
    client.StartClient("127.0.0.1", 4433, "", "");
    client.buy("SRD-BTC", 50);
    client.closeConnection();
    return 0;
}
