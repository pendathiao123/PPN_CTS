#include <iostream>
#include "Server.h"
#include "Client.h"
#include "Crypto.h"
int main() {
    Client client("127.0.0.1", 8080);
    client.getMarket();

    client.getBalance("SRD-BTC");
    client.getBalance("DOLLARS");
    //Achat
    client.buy("SRD-BTC", 5.0);
    client.getBalance("SRD-BTC");
    client.getBalance("DOLLARS");
    //Vente 
    client.sell("SRD-BTC", 5.0);
    client.getBalance("SRD-BTC");
    client.getBalance("DOLLARS");
    //Vente 
    client.sell("SRD-BTC", 5.0);
    client.getBalance("SRD-BTC");
    client.getBalance("DOLLARS");
    //Vente 
    client.sell("SRD-BTC", 5.0);
    client.getBalance("SRD-BTC");
    client.getBalance("DOLLARS");

    return 0;
}