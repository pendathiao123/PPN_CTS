#ifndef _SERVER_TRANSACTION_H_
#define _SERVER_TRANSACTION_H_


#include <iostream>
#include <string>
#include <fstream>
#include <ctime>


class Server_Transaction{
private:

    int trans_ID; // Identifiant unique pour chaque transaction
    std::time_t timestamp; // Timestamp pour gerer l'écriture des transactions
    const std::string filename; // fichier dans lequels on écrit les transactions
    const std::string fileformat = "ID,Type,Crypto-Name,Quantity,UnitPrice,TotalAmount,Timestamp\n";

public:
    //Constructeur: prend en argument le fichier où on écrit les transacations (initialise le timestamp)
    Server_Transaction(std::string file);
    //Destructeur
    ~Server_Transaction() = default;


    /* Fonction qu'on mettre dans une librairie */
    void add_p(std::string& ss);

    /* Méthodes */
    // Lire une transaction (format d'une transaction qui reste à définir, pour l'instant du texte)
    std::string readTransaction(int i); //prend en argument le numero de la transaction à lire
    // Lire la transaction la plus récente
    std::string readTransaction();
    // Enregistrer la transaction dans le fichier txt filename, prend en argument le format txt d'une transaction
    void writeTransaction(const std::string transaction_to_write);
    // Tanscrire sous format string les infos d'une transaction
    std::string transactionToString(int type, std::string nomCrypto, int q, double prixUnit);


    // Démarage du serveur ...
    void start();
};

#endif // _SERVER_TRANSACTION_H_