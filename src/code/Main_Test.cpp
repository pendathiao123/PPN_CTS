#include <iostream>
#include "../headers/Server.h" // Server_Transaction est inclus ici !
#include "../headers/Client.h"

/* TESTS pour la classe Server_Transaction */
const std::string filename_t = "../../data/transactions.txt";
// attention à l'endroit où on execute ce programme !!

void test_transaction(Server_Transaction& st){ // on ne veut pas recopier le contenu de la classe ici: on passe par reference
    st.writeTransaction(st.transactionToString(0,"SRD-BTC",50,10));
    st.writeTransaction(st.transactionToString(1,"SRD-BTC",4,10));
    std::cout << st.readTransaction(0) << "\n";
    std::cout << st.readTransaction(1) << "\n"; // on va lire la première transaction
    st.writeTransaction(st.transactionToString(1,"SRD-BTC",23,10));
    std::cout << st.readTransaction(-1) << "\n"; // on va lire tout le fichier !
}

int main(){
    Server_Transaction servt{filename_t};
    test_transaction(servt);
    return 0;
}

// aranger les reads !!