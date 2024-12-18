#include <iostream>
#include "../headers/Client.h"
#include "../headers/bot.h"


int main() {
    Client client("127.0.0.1", 8080);
    bot bot;
    bot.buyCrypto("SRD-BTC", 10);
    bot.sellCrypto("SRD-BTC",20);
    return 0;
}