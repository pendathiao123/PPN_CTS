#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"
#include "../headers/bot.h"


int main() {
    Client client("127.0.0.1", 8080);
    client.buy("SRD-BTC", 10);
    return 0;
}