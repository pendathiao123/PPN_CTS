#ifndef CLIENT_H
#define CLIENT_H

#include <string>

class Client {
private:

    int idC; //identifiant unique du Client
    static int idC_counter;  // Compteur statique pour générer des ID des clients

    int clientSocket;
    //adresse IP: ??

    struct Portfolio {
        // Quantité détenue par le client: dictionnaire de <nom cypto, quantité>
        std::map<std::string, double> holdings;

        Portfolio(const std::string cryptos);//Constructeur du Portfolio
        void buyCrypto(const std::string& name, double amount);
        void sellCrypto(const std::string& name, double amount);
        //Retourne la quantité d'une cryptomonnaie détenue dans le portefeuille
        double getBalance(const std::string& name) const; 
    };

public:
    Client(const std::string& address, int port);
    void sendRequest(const std::string& request);
    std::string receiveResponse();  // Nouvelle méthode pour recevoir une réponse

    ~Client();  // Déclaration du destructeur
};

#endif  // CLIENT_H
