#ifndef _SERVER_TRANSACTION_H_
#define _SERVER_TRANSACTION_H_


#include <iostream>
#include <string>
#include <fstream>
#include <ctime>


class Server_Transaction{
private:

    std::time_t timestamp; // Timestamp pour gerer l'écriture des transactions
    const std::string filename; // fichier dans lequels on écrit les transactions

public:
    //Constructeur: prend en argument le fichier où on écrit les transacations (initialise le timestamp)
    Server_Transaction(std::string file);
    //Destructeur
    ~Server_Transaction();

    /* Guetteurs */
    // donne le temps
    std::time_t getTimestamp() const;

    /* Méthodes */
    // Lire une transaction (format d'une transaction qui reste à définir, pour l'instant du texte)
    std::string readTransaction(int i); //prend en argument le numero de la transaction à lire
    // Lire la transaction la plus récente
    std::string readTransaction();
    // Enregistrer la transaction dans le fichier CSV, prend en argument le format txt d'une transaction
    void writeTransaction(const std::string transaction_to_write);
    
    // Démarage du serveur ...
    void start();
};

#endif // _SERVER_TRANSACTION_H_