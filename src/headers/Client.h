#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <map>


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
        void buyCrypto(const std::string& currency, double pourcentage);
        void sellCrypto(const std::string& currency, double pourcentage);
        //Retourne la quantité d'une cryptomonnaie détenue dans le portefeuille
        double getBalance(const std::string& name) const; 
    };

public:
    Client();
    Client(const std::string& address, int port);
    void sendRequest(const std::string& request);
    int getsocket();
    std::string receiveResponse();  // Nouvelle méthode pour recevoir une réponse
    void Request(int clientSocket);
    ~Client();  // Déclaration du destructeur
    void buy(const std::string& currency, double percentage);
    void sell(const std::string& currency, double percentage);
};

#endif  // CLIENT_H
