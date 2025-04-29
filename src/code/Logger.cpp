#include "../headers/Logger.h"

// Constructeur privé
Logger::Logger(const std::string& filename) {
    logFile.open(filename, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier de log " << filename << std::endl;
    } else {
        logFile << "--- Début du log : " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << " ---\n";
    }
}

// Destructeur
Logger::~Logger() {
     if (logFile.is_open()) {
        logFile << "--- Fin du log : " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << " ---\n";
        logFile.close();
     }
}

// Implémentation du singleton
Logger& Logger::getInstance() {
    // 'static' garantit qu'une seule instance est créée et initialisée de manière thread-safe (depuis C++11)
    static Logger instance;
    return instance;
}

// Convertit un niveau de log enum en chaîne de caractères
std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// Fonction pour convertir une chaîne de caractères ("INFO", "ERROR") en niveau de log enum
LogLevel logLevelFromString(const std::string& level_str) {
    if (level_str == "DEBUG") return LogLevel::DEBUG;
    else if (level_str == "INFO") return LogLevel::INFO;
    else if (level_str == "WARNING") return LogLevel::WARNING;
    else if (level_str == "ERROR") return LogLevel::ERROR;
    else {
        // Si la chaîne n'est pas reconnue, retourner un niveau par défaut
        // Éviter d'utiliser le Logger ici pour ne pas créer de récursion infinie si l'erreur survient dans un appel au Logger.
        std::cerr << "[Logger ERROR] Niveau de log invalide spécifié : '" << level_str << "'. Utilisation du niveau INFO par défaut." << std::endl;
        return LogLevel::INFO; // Niveau par défaut
    }
}


// Méthode principale pour logguer
// Elle reçoit le niveau déjà en enum
void Logger::log(LogLevel level, const std::string& message) {
    // Utiliser un lock_guard pour s'assurer qu'un seul thread écrit à la fois
    std::lock_guard<std::mutex> lock(mtx);

    if (!logFile.is_open()) {
        // Si le fichier n'est pas ouvert, écrire sur stderr (ou une autre gestion d'erreur)
        std::cerr << "[LOG FILE ERROR] " << message << std::endl;
        return; // Sortie si le fichier n'est pas prêt
    }

    // Obtenir le timestamp actuel
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    // Convertir en struct tm (utiliser la version thread-safe si disponible)
    std::tm timeinfo_buffer;
    std::tm* timeinfo = localtime_r(&time_t, &timeinfo_buffer); // POSIX thread-safe

    std::stringstream ss;
    if (timeinfo) {
        ss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
    } else {
         ss << "[TIMESTAMP_ERROR]";
    }

    // Écrire le message formaté dans le fichier
    logFile << "[" << ss.str() << "][" << logLevelToString(level) << "] " << message << std::endl;

    // Optionnel : écrire aussi sur la console selon le niveau
    if (level == LogLevel::WARNING || level == LogLevel::ERROR) {
        std::cerr << "[" << ss.str() << "][" << logLevelToString(level) << "] " << message << std::endl;
    }
     // Optionnel : écrire INFO/DEBUG sur stdout
    else if (level == LogLevel::INFO || level == LogLevel::DEBUG) {
         std::cout << "[" << ss.str() << "][" << logLevelToString(level) << "] " << message << std::endl;
    }
}