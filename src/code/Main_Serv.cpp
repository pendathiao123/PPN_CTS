#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"
#include "../headers/Global.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>

// Fonction pour démarrer le gestionnaire de BTC
void startBTCManager()
{
    const std::string filename = "../src/data/btc_data.csv";
    const std::string btcSecFilename = "../src/data/btc_sec_values.csv";

    // Boucle continue jusqu'à ce que l'arrêt soit demandé
    while (!Global::getStopRequested())
    {
        Global::populateBTCValuesFromCSV(filename);           // Remplir les valeurs BTC quotidiennes à partir d'un fichier CSV
        Global::Complete_BTC_value();                         // Compléter les valeurs BTC pour chaque seconde de la journée
        Global::writeBTCValuesToCSV(btcSecFilename);          // Écrire les valeurs BTC dans un fichier CSV
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Pause de 1 seconde entre les cycles
    }
}

// Fonction pour démarrer le serveur
void startServer()
{
    const std::string certFile = "../server.crt";
    const std::string keyFile = "../server.key";
    const std::string usersFile = "../configFile.csv";
    const std::string logFile = "../log.csv";
    int port = 4433;

    Server server;
    server.StartServer(port, certFile, keyFile, usersFile, logFile); // Démarrer le serveur avec les fichiers de configuration
}

// Fonction principale
int main()
{
    pid_t pid = fork(); // Créer un nouveau processus

    if (pid < 0)
    {
        std::cerr << "Erreur de création du processus" << std::endl;
        return 1;
    }

    if (pid == 0)
    {
        // Processus enfant pour gérer le BTC
        startBTCManager();
    }
    else
    {
        // Processus parent pour gérer le serveur
        startServer();

        // Attendre la fin du processus enfant
        wait(NULL);
    }
    return 0;
}