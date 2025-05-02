#include <cstdlib> // Pour rand() et srand()
#include <ctime>   // Pour time()
#include <fstream>
#include <atomic>
#include <chrono>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include "../headers/Crypto.h"
//#include "../headers/SRD_BTC.h"

// Constructeurs
Crypto::Crypto() : name(""), price(0.0), changeRate(0.0), balances(balances) {
    current_value = 1;
}
Crypto::Crypto(const std::string &name, double initialPrice, double changeRate)
    : name(name), price(initialPrice), changeRate(changeRate), balances(balances) {
    current_value = 1;
}

// Retourne le nom de la crypto
std::string Crypto::getName() const
{
    return name;
}

// Fonction pour obtenir la valeur complète du BTC pour une seconde donnée le jour 0 à partir d'un fichier CSV
double Crypto::get_SRD_BTC_value()
{
    const std::string filePath = "../src/data/btc_sec_values.csv";
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filePath << "\n";
        return 0.0;
    }

    std::string line;
    int day, second, i=1;
    double value;

    // Ignorer la ligne d'en-tête
    std::getline(file, line);

    while (std::getline(file, line))
    {
        std::istringstream lineStream(line);
        std::string cell;
        int cellIndex = 0;

        if(i == current_value){ // si on se trouve sur la ligne souhaité
            while (std::getline(lineStream, cell, ',')) // on lit la ligne
            {
                switch (cellIndex)
                {
                case 0: // extraction du jour
                    day = std::stoi(cell);
                    break;
                case 1: // extraction de la seconde
                    second = std::stoi(cell);
                    break;
                case 2: // extraction de la valeur
                    try
                    {
                        value = std::stod(cell);
                        // Vérification de la validité de la valeur
                        if (std::isinf(value) || std::isnan(value) || value < 1e-10)
                        {
                            std::cerr << "Valeur invalide (inf, nan, ou trop petite) dans la cellule : " << cell << std::endl;
                            value = 0.0; // Valeur par défaut en cas d'erreur
                        }
                    }
                    catch (const std::invalid_argument &e)
                    {
                        std::cerr << "Format de nombre invalide dans la cellule : " << cell << std::endl;
                        value = 0.0; // Valeur par défaut en cas d'erreur
                    }
                    catch (const std::out_of_range &e)
                    {
                        std::cerr << "Nombre hors de portée dans la cellule : " << cell << std::endl;
                        value = 0.0; // Valeur par défaut en cas d'erreur
                    }
                    break;
                }
                ++cellIndex;
            }
            
            // on sort de la fonction
            file.close();
            return value;

        }else{ // sinon on passe à la ligne suivante
            ++i;
        }
    }

    //std::cerr << "Erreur : Clé {0, " << t << "} non trouvée dans " << filePath << ".\n";
    std::cerr << "La ligne " << current_value << " n'est pas dans le fichier";
    file.close();
    return 0.0;
}


// Retourne le prix actuel de la crypto
double Crypto::getPrice(const std::string &currency)
{
    /*    
    std::time_t currentTime = std::time(0);
    std::tm *now = std::localtime(&currentTime);
    int day = now->tm_yday;
    int second = now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec;
    */
    if (currency == "SRD-BTC")
    {
        // Global::readBTCValuesFromCSV("btc_sec_values.csv");
        return get_SRD_BTC_value();
    }
    else
    {
        return 0.0;
    }
}
/*
double Crypto::getPrice(const std::string &currency) const
{
    /*
    for(const auto& [crypto, value] : cryptoCurrency){
        if(crypto == currency){
            return value.back(); // on retourne la dernière valeur ajouté
        }
    }
    
    if (currency == "SRD-BTC")
    {
        double price = 45.00 + static_cast<double>(rand());
        return price;
    }
    else
    {
        return false;
    }
    
    //std::cout << "Server: Crypto renseigné non présente en mémoire !!\n" << std::endl;
    //return 0.;
}
*/

// Met à jour le prix en fonction du taux de variation
void Crypto::updatePrice()
{
    price += price * (changeRate / 100); // Le prix augmente selon le taux de variation
}

// Affiche les informations sur la crypto
void Crypto::displayInfo() const
{
    std::cout << "Crypto: " << name << ", Prix: " << price << ", Taux de variation: " << changeRate << "%" << std::endl;
}

// Méthode pour récupérer le dernier prix enregistré (par exemple, de fichier ou base de données)
double Crypto::get_prv_price(const std::string &currency)
{
    // Logique pour obtenir le dernier prix enregistré
    std::ifstream inFile("SRD-BTC.dat");
    if (!inFile)
    {
        std::cerr << "Erreur lors de l'ouverture du fichier SRD-BTC.dat" << std::endl;
        return -1;
    }

    double price;
    std::string line;
    while (std::getline(inFile, line))
    {
        // Lecture du fichier pour obtenir le dernier prix
        size_t pos = line.find_last_of(' ');
        if (pos != std::string::npos)
        {
            price = std::stod(line.substr(pos + 1));
        }
    }
    return price; // Retourner le dernier prix enregistré
}

// Méthode qui fait evouler le prix des cryptos à chaque appel
void Crypto::retroActivitySim(){
    current_value++;
}