#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <ctime>

class Transaction
{
public:
    Transaction(const std::string &clientId, const std::string &type, const std::string &cryptoName, double quantity, double unitPrice);
    void logTransactionToCSV(const std::string &filename) const;
    std::string getId() const;
    static std::string readTransaction(int i, const std::string &filename);

private:
    std::string id;
    std::string clientId;
    std::string type;
    std::string cryptoName;
    double quantity;
    double unitPrice;
    double totalAmount;
    std::time_t timestamp;
    static int counter;
};

#endif // TRANSACTION_H