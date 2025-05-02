#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <limits>
#include <random>
#include <iostream>
#include <cmath>
#include <random>
#include "../headers/Global.h"

std::atomic<bool> Global::stopRequested = false;
std::array<double, 10> Global::BTC_daily_values = {};

std::atomic<bool> &Global::getStopRequested()
{
    return stopRequested;
}

std::array<double, 10> &Global::getBTCDailyValues()
{
    return BTC_daily_values;
}

// Remplit BTC_daily_values à partir d'un fichier CSV
void Global::populateBTCValuesFromCSV(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << "\n";
        return;
    }

    std::string line;
    int dayIndex = 0;

    // Ignorer la première ligne (en-tête)
    std::getline(file, line);

    while (std::getline(file, line) && dayIndex < BTC_daily_values.size())
    {
        std::istringstream lineStream(line);
        std::string cell;
        int cellIndex = 0;

        while (std::getline(lineStream, cell, ','))
        {
            // Supprimer les guillemets de la valeur de la cellule
            cell.erase(std::remove(cell.begin(), cell.end(), '\"'), cell.end());

            if (cellIndex == 1)
            { // La valeur "Dernier" est dans la deuxième cellule (index 1)
                try
                {
                    BTC_daily_values[dayIndex] = std::stod(cell);
                    //std::cout << "BTC_daily_value[" << dayIndex << "] = " << BTC_daily_values[dayIndex] << std::endl; // Message de débogage
                }
                catch (const std::invalid_argument &e)
                {
                    std::cerr << "Format de nombre invalide dans la cellule : " << cell << std::endl;
                }
                catch (const std::out_of_range &e)
                {
                    std::cerr << "Nombre hors de portée dans la cellule : " << cell << std::endl;
                }
                break; // Passer à la ligne suivante après avoir trouvé la valeur "Dernier"
            }
            ++cellIndex;
        }
        ++dayIndex;
    }

    file.close();
}

// Écrit les valeurs BTC dans un fichier CSV
void Global::writeBTCValuesToCSV(const std::string &filename)
{
    std::ofstream file(filename);

    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << "\n";
        return;
    }

    // Écrire l'en-tête du fichier CSV
    file << "Day,Second,Value\n";

    for (int d = 0; d < 1; ++d)
    {
        float BTC_value = BTC_daily_values[d];
        for (int t = 0; t < 100; ++t)
        {
            file << d << "," << t << "," << BTC_value << "\n";
        }
    }

    file.close();
}

// Lit les valeurs BTC à partir d'un fichier CSV
void Global::readBTCValuesFromCSV(const std::string &filename)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << "\n";
        return;
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
                    if (std::isinf(value) || std::isnan(value))
                    {
                        std::cerr << "Valeur invalide (inf ou nan) dans la cellule : " << cell << std::endl;
                        value = 0.0; // Valeur par défaut en cas d'erreur
                    }
                    //std::cout << "Valeur lue : " << value << std::endl; // Message de débogage
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
    }

    file.close();
}

// Affiche les valeurs BTC pour un jour donné, entre deux secondes spécifiques
void Global::printBTCValuesForDay(int day, int start_second, int end_second)
{
    std::ifstream file("../src/data/btc_sec_values.csv");

    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier btc_sec_values.csv\n";
        return;
    }

    std::string line;
    int d, second;
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
                d = std::stoi(cell);
                break;
            case 1:
                second = std::stoi(cell);
                break;
            case 2:
                try
                {
                    value = std::stod(cell);
                    // Vérification de la validité de la valeur
                    if (std::isinf(value) || std::isnan(value))
                    {
                        std::cerr << "Valeur invalide (inf ou nan) dans la cellule : " << cell << std::endl;
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

        if (d == day && second >= start_second && second <= end_second)
        {
            std::cout << "{" << d << ", " << second << "} : " << value << "\n";
        }
    }

    file.close();
}


double Global::getRandomDouble() {
    // Utiliser un générateur de nombres aléatoires
    std::random_device rd;
    std::mt19937 gen(rd());  // Mersenne Twister pour une meilleure qualité d'aléatoire

    // Distribution normale centrée en 0.02 et écart-type petit pour contenir les résultats dans [0, 0.004]
    std::normal_distribution<> dis(0.02, 0.004);  // Moyenne = 0.02, écart-type = 0.004

    // Générer un nombre normal dans la plage [0, 0.1]
    double result = dis(gen);

    // Si le résultat dépasse 0.04 ou est inférieur à 0, on le recale pour qu'il reste dans la plage souhaitée
    if (result > 0.04) result = 0.04;
    if (result < 0.0) result = 0.0;

    return result;
}


// Retourne la valeur quotidienne du BTC pour un jour donné
float Global::get_daily_BTC_value(int d)
{
    const auto &BTC_daily_values = Global::getBTCDailyValues();
    if (d < 0 || d >= BTC_daily_values.size())
    {
        std::cerr << "Erreur : Index " << d << " hors des limites pour BTC_daily_values.\n";
        return 0.0;
    }
    double BTC_value = BTC_daily_values[d];
    return static_cast<float>(BTC_value);
}

// Complète les valeurs BTC pour chaque seconde de la journée et les écrit dans un fichier CSV
void Global::Complete_BTC_value()
{
    std::ofstream file("../src/data/btc_sec_values.csv");
    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier btc_sec_values.csv\n";
        return;
    }

    // Écrire l'en-tête du fichier CSV
    file << "Day,Second,Value\n";

    for (int d = 0; d < 1; ++d)
    {
        double BTC_value = get_daily_BTC_value(d);
        for (int t = 0; t < 86400; ++t)
        {
            double randomValue = getRandomDouble(); // Générer une valeur aléatoire

            BTC_value = (0.98 +randomValue)  * BTC_value;

       
            file << d << "," << t << "," << BTC_value << "\n";

            // Message de débogage pour vérifier les valeurs de BTC_value
            if (t % 3600 == 0)
            { // Imprimer les valeurs toutes les heures
                //std::cout << "Jour : " << d << ", Seconde : " << t << ", Valeur BTC : " << BTC_value << std::endl;
            }
        }
    }

    file.close();
}