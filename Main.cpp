#include <iostream>
#include "Server.h"
#include "Client.h"
#include "Transaction.h"

int main() {

    Transaction tx("buy", "Bitcoin", 0.5, 30000.0);
    Transaction::logTransactionToCSV(tx,"log.csv");

    return 0;
}
