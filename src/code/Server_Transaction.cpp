#include "../headers/Server_Transaction.h"



Server_Transaction::Server_Transaction(std::string file) : filename(file){}


std::time_t Server_Transaction::getTimestamp() const { return timestamp; }

std::string Server_Transaction::readTransaction(int i){
    /* On suppose que chaque transaction occupe une ligne,
    alors il suffit d'aller a la ligne i+1 (car la première ligne est resevée) 
    pour lire la transaction desirée
    - Si i vaut 0, on retourne la transaction la plus recente
    - Si i est positif on retourne la i+1 ième ligne du doc
    - Si i est négatif on renvoie toutes les transaction enregistrées
        (on pourrait envisager de majorer cela a un nombre defini constant,
        étant donné le nombre de transactions qui pourront être enregistrés)*/
    std::ifstream file(filename, std::ios::in);
    if(file){
        std::string buffer;
        if(i == 0){ // on va à la dernière ligne
            while(getline(file,buffer)){} // tant que l'on peut mettre la ligne dans buffer
            // à la fin buffer contient la dernière ligne
            file.close();
            return buffer;
        }else if(i > 0){ // on va a la ligne i!
            while((getline(file,buffer)) && (i > 0)){
                i--;
            }
            file.close();
            return buffer;
        }else{
            std::string res;
            while(getline(file,buffer)){
                res += buffer;
            }
            file.close();
            return res;
        }
    }else{
        std::cerr << "Erreur : Impossible d'ouvrir le fichier en lecture \n" << std::endl;
    }
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