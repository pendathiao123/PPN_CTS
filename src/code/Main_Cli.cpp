#include <iostream>
#include "../headers/Client.h"
#include "../headers/Bot.h"

int main()
{
    Client client;
    client.StartClient("127.0.0.1", 4433, "1206", "4aeeb5ffc3be24aae06916942890aad97b6bb81572cdc05ee1a3462186675057");
    return 0;
}
