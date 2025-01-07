#include <iostream>
#include <random>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include "../headers/Global.h" // Inclusion du fichier d'en-tête
#include "../headers/SRD_BTC.h"

// Fonction pour obtenir la valeur complète du BTC pour une seconde donnée le jour 0 à partir d'un fichier CSV
double get_complete_BTC_value(int d, int t)
{
    const std::string filePath = "../src/data/btc_sec_values.csv";
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filePath << "\n";
        return 0.0;
    }

    std::string line;
    int day, second;
    double value;

    // Ignorer la ligne d'en-tête
    std::getline(file, line);

    while (std::getline(file, line))
    {
        std::istringstream lineStream(line);
        std::string cell;
        int cellIndex = 0;

        while (std::getline(lineStream, cell, ','))
        {
            switch (cellIndex)
            {
            case 0:
                day = std::stoi(cell);
                break;
            case 1:
                second = std::stoi(cell);
                break;
            case 2:
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

        // Rechercher uniquement pour le jour 0
        if (day == 0 && second == t)
        {
            file.close();
            return value;
        }
    }

    std::cerr << "Erreur : Clé {0, " << t << "} non trouvée dans " << filePath << ".\n";
    file.close();
    return 0.0;
}

// Fonction pour afficher la valeur du SRD-BTC
void SRD_BTC()
{
    double randomFloat = Global::getRandomDouble();
    std::ifstream file("../src/data/btc_sec_values.csv");
    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier btc_sec_values.csv\n";
        return;
    }

    std::string line;
    int day, second;
    double value;
    double SRD_BTC_value = 0;

    // Ignorer la ligne d'en-tête
    std::getline(file, line);

    while (std::getline(file, line))
    {
        std::istringstream lineStream(line);
        std::string cell;
        int cellIndex = 0;

        while (std::getline(lineStream, cell, ','))
        {
            switch (cellIndex)
            {
            case 0:
                day = std::stoi(cell);
                break;
            case 1:
                second = std::stoi(cell);
                break;
            case 2:
                value = std::stod(cell);
                break;
            }
            ++cellIndex;
        }

        // Calculer la valeur du SRD-BTC
        SRD_BTC_value = (0.9489 + randomFloat) * value;
        std::cout << SRD_BTC_value << "\n";
    }

    file.close();
}