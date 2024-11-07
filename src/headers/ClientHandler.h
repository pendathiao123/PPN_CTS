class ClientHandler {
private:
    int clientSocket;  // Socket associé au client
    std::unordered_map<std::string, double>& balances;
    Crypto cryptoInstance;

public:
    ClientHandler(int clientSocket, std::unordered_map<std::string, double>& balances); // Constructeur avec utilisation de référence vers les balances du client

    std::string handleMarket(const std::string& request);
    std::string handleBuy(const std::string& request);
    std::string handleSell(const std::string& request);
    std::string handleBalance(const std::string& currency);
    
    void processRequest();  // Traite les commandes du client
    void sendResponse(const std::string& response);  // Envoie une réponse au client
};