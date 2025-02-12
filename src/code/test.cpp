#include "../headers/Bot.h"
#include "../headers/Client.h"
#include "../headers/Global.h"
#include "../headers/SRD_BTC.h"
#include "../headers/Crypto.h"
#include "../headers/Server.h"
#include "../headers/Transaction.h"
#include <cassert>


/* TEST DES FONCTIONS */


void test_getBalance() {
    Bot bot;
    assert(bot.getBalance("DOLLARS") == 10000.0);
    assert(bot.getBalance("SRD-BTC") == 0.0);
    assert(bot.getBalance("EUR") == 0.0); // Devrait retourner 0 pour une devise inexistante
    std::cout << "test_getBalance passed!" << std::endl;
}

void test_updateBalance() {
    Bot bot;
    std::unordered_map<std::string, double> newBalances = {
        {"DOLLARS", 5000.0},
        {"SRD-BTC", 2.5}
    };
    bot.updateBalance(newBalances);
    assert(bot.getBalance("DOLLARS") == 5000.0);
    assert(bot.getBalance("SRD-BTC") == 2.5);
    std::unordered_map<std::string, double> newBalances = {
        {"DOLLARS", 2.5},
        {"SRD-BTC", 500.0}
    };
    assert(bot.getBalance("DOLLARS") == 2.5);
    assert(bot.getBalance("SRD-BTC") == 500.0);
    std::cout << "test_updateBalance passed!" << std::endl;
}

void test_get_total_Balance() {
    Bot bot;
    std::unordered_map<std::string, double> balances = bot.get_total_Balance();
    assert(balances["DOLLARS"] == 10000.0);
    assert(balances["SRD-BTC"] == 0.0);
    std::cout << "test_get_total_Balance passed!" << std::endl;
}

void test_getName() {
    Crypto crypto("SRD-BTC", 50.00, 0.5);
    assert(crypto.getName() == "SRD-BTC");
    std::cout << "test_getName passed!" << std::endl;
}

void test_getPrice() {
    Crypto crypto("SRD-BTC", 50.00, 0.5);
    assert(crypto.getPrice("SRD-BTC") > 0.0);
    std::cout << "test_getPrice passed!" << std::endl;
}

void test_updatePrice() {
    Crypto crypto("SRD-BTC", 50.00, 0.5);
    double oldPrice = crypto.getPrice("SRD-BTC");
    crypto.updatePrice();
    assert(crypto.getPrice("SRD-BTC") != oldPrice);
    std::cout << "test_updatePrice passed!" << std::endl;
}

void test_get_prv_price() {
    std::ofstream outFile("SRD-BTC.dat");
    outFile << "Previous Price: 100.00";
    outFile.close();
    
    Crypto crypto;
    assert(crypto.get_prv_price("SRD-BTC") == 100.00);

    std::ofstream outFile("SRD-BTC.dat");
    outFile << "Previous Price: 120.00";
    outFile.close();
    
    Crypto crypto;
    assert(crypto.get_prv_price("SRD-BTC") == 120.00);
    std::cout << "test_get_prv_price passed!" << std::endl;
}

void test_buyCrypto() {
    Bot bot;
    bot.buyCrypto("SRD-BTC", 10);  // Achat de 10% du solde en BTC
    assert(bot.getBalance("DOLLARS") == 9000.0);  
    assert(bot.getBalance("SRD-BTC") > 0.0);  // Cet encadrement est nécessaire, car la valeur obtenue dépend de celle de la monnaie
    std::cout << "test_buyCrypto passed!" << std::endl;
}

void test_sellCrypto() {
    Bot bot;
    bot.buyCrypto("SRD-BTC", 10);
    float a = bot.getBalance("SRD-BTC");
    bot.sellCrypto("SRD-BTC", 5);  // Vente de 5% des SRD-BTC achetés (sur 10)
    assert(bot.getBalance("SRD-BTC") >= 0.0 || bot.getBalance("SRD-BTC") < a);  // La valeur doit être comprise entre 10% et positive
    std::cout << "test_sellCrypto passed!" << std::endl;
}

void test_trading() {
    Bot bot;
    bot.trading(); // Simulation d'un cycle de trading
    std::cout << "test_trading executed!" << std::endl;
}

int main() {
    test_getBalance();
    test_updateBalance();
    test_get_total_Balance();
    test_getName();
    test_getPrice();
    test_updatePrice();
    test_get_prv_price();
    test_buyCrypto();
    test_sellCrypto();
    test_trading();
    std::cout << "Tests passés avec succès!" << std::endl;
    return 0;
}
