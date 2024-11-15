#include "../headers/Server_Transaction.h"



std::time_t Server_Transaction::getTimestamp() const { return timestamp; }



std::string Server_Transaction::readTransaction(int i){
    /* On suppose que chaque transaction occupe une ligne,
    alors il suffit d'aller a la ligne i+1 pour lire la transaction desirée
    Si i vaut 0, on retourne la transaction la plus recente */
}

std::string Server_Transaction::readTransaction(){
    return readTransaction(0);
}

void Server_Transaction::writeTransaction(const std::string transaction_to_write) {
    std::ofstream file(filename, std::ios::app);
    if (file.is_open()) {
        // Vérifie si le fichier est vide pour écrire l'en-tête
        if (file.tellp() == 0) {
            file << "ID,Type,CryptoName,Quantity,UnitPrice,TotalAmount,Timestamp\n";  // Écrire l'en-tête
        }
    
        // Convertir le timestamp en date lisible
        std::tm *tm = std::localtime(&timestamp);
        char readableTimestamp[20];
        std::strftime(readableTimestamp, sizeof(readableTimestamp), "%Y-%m-%d %H:%M:%S", tm); // Formatage

        // Écrire les données de la transaction 
        file << readableTimestamp << transaction_to_write << "\n";
        file.close();
    } else {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << transaction_to_write << std::endl;
    }
}

void Server_Transaction::start(){
    // à completer ...
}