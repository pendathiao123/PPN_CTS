#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

// Enumération des niveaux de log
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

// Convertit un niveau de log enum en chaîne de caractères (utile en interne dans Logger.cpp)
std::string logLevelToString(LogLevel level);

// NOUVELLE fonction pour convertir une chaîne de caractères ("INFO", "ERROR") en niveau de log enum
LogLevel logLevelFromString(const std::string& level_str);


class Logger {
private:
    std::ofstream logFile;
    std::mutex mtx; // Mutex pour assurer la thread-safety

    // Empêcher la copie et l'assignation (Singleton)
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Constructeur privé pour implémenter le singleton
    // Utilise un nom de fichier par défaut ou peut être configuré
    Logger(const std::string& filename = "server.log");
    ~Logger();


public:
    // Méthode statique pour obtenir l'instance unique du Logger (Singleton)
    static Logger& getInstance();

    // Méthode pour logguer un message
    // Le premier argument est le niveau (enum), le second est le message
    void log(LogLevel level, const std::string& message);

    // Méthodes utilitaires pour logguer facilement avec un niveau prédéfini
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
};

// Macro simplifiée pour l'utilisation
// Elle prend le message et la CHAÎNE DE CARACTÈRES du niveau ("INFO", "ERROR", etc.)
#define LOG(message, level_str) \
    do { \
        /* Utilise la fonction pour convertir la chaîne en enum */ \
        LogLevel level_enum = logLevelFromString(level_str); \
        /* Appelle la méthode log() avec le niveau enum et le message */ \
        Logger::getInstance().log(level_enum, (message)); \
    } while(0)


#endif