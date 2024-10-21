class ClientHandler {
private:
    int clientSocket;  // Socket associé au client

public:
    ClientHandler(int socket);
    void processRequest(const std::string& request);  // Traite les commandes du client
    void sendResponse(const std::string& response);  // Envoie une réponse au client
};
