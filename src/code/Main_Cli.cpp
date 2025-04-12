#include <iostream>
#include <thread>
#include <chrono>
#include "../headers/Client.h"
#include "../headers/Bot.h"

Bot bot;

int main() {
    Client client;
    client.StartClient("127.0.0.1", 4433, "7474", "77d7728205464e7791c58e510d613566874342c26413f970c45d7e2bc6dd9836");
    client.buy("SRD-BTC", 50);
    while(true) {
        double a = bot.getBalance("SRD-BTC");
        double c = bot.getBalance("DOLLARS");
        
        std::cout << a << "SRD-BTC" << c << "DOLLARS" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(120));
    }
    return 0;
}
