#include "../headers/Server_Transaction.h"



Server_Transaction::Server_Transaction(std::string file) : filename(file){
    trans_ID = 0;
    timestamp = std::time(nullptr);
}



void Server_Transaction::add_p(std::string& ss){
    ss.push_back(',');
}

std::string Server_Transaction::transactionToString(int type, std::string nomCrypto, int q, double prixUnit){
    // Cette section de code peut être amené a être modifié
    std::string trans = std::to_string(trans_ID); // identifiant de la transaction
    add_p(trans);
    ++trans_ID;
    if(type){ // si c'est 1 (vrai) -> Achat
        trans += "Buy,";
    }else{ // si c'est 0 (faux) -> Vente
        trans += "Sell,";
    }
    trans += nomCrypto;
    add_p(trans);
    trans += std::to_string(q); // quantité de crypto
    add_p(trans);
    trans += std::to_string(prixUnit); // prix de la crypto à l'unité
    add_p(trans);
    trans += std::to_string(q*prixUnit); // prix total de la transaction
    return trans;
}

void Server_Transaction::writeTransaction(const std::string transaction_to_write){
    std::ofstream file(filename, std::ios::app);
    if (file.is_open()) {
        // Vérifie si le fichier est vide pour écrire l'en-tête
        if (file.tellp() == 0) {
            file << fileformat;  // Écrire l'en-tête
        }
    
        // Convertir le timestamp en date lisible
        std::tm *tm = std::localtime(&timestamp);
        char readableTimestamp[256];
        std::strftime(readableTimestamp, sizeof(readableTimestamp), "%d-%m-%Y %H:%M:%S", tm); // Formatage

        // Écrire les données de la transaction 
        file << transaction_to_write << "," << readableTimestamp << "\n";
        file.close();
    } else {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier en écriture " << std::endl;
    }
}

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
        // à la fin buffer contient la dernière ligne lue correctement !
    }else{
        std::cerr << "Erreur : Impossible d'ouvrir le fichier en lecture \n" << std::endl;
        return std::to_string(0);
    }
}

std::string Server_Transaction::readTransaction(){
    return readTransaction(0);
}


void Server_Transaction::start(){
    // à completer ...
}