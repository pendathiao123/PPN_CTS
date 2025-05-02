#ifndef BOLLINGER_BOT_H // Include guard - évite les inclusions multiples
#define BOLLINGER_BOT_H

#include <vector> // Nécessaire pour std::vector dans la déclaration de la classe

// --- Structure pour contenir les valeurs des Bandes de Bollinger ---
// Reste ici car c'est utilisé par l'interface publique (getLastBands)
struct BollingerBands {
    double middle = 0.0;
    double upper = 0.0;
    double lower = 0.0;
    bool valid = false;
};

// --- Enum pour les actions possibles ---
// Reste ici car c'est utilisé par l'interface publique (processNewPrice)
enum class TradingAction {
    HOLD,
    BUY,
    SELL,
    CLOSE_LONG,
    CLOSE_SHORT
};

// --- Enum pour l'état actuel de la position ---
// Reste ici car c'est utilisé par l'interface publique (getCurrentState)
enum class PositionState {
    NONE,
    LONG,
    SHORT
};


// --- Déclaration de la Classe du Bot Bollinger ---
class BollingerBot {
public:
    // --- Constructeur ---
    // Déclare le constructeur (l'implémentation sera dans le .cpp)
    BollingerBot(int p, double k_stddev);

    // --- Méthode principale pour traiter un nouveau prix ---
    // Déclare la méthode (l'implémentation sera dans le .cpp)
    TradingAction processNewPrice(double newPrice);

    // --- Getters pour l'état et les informations ---
    PositionState getCurrentState() const; // Ajout de 'const' car ne modifie pas l'état
    double getEntryPrice() const;        // Ajout de 'const'
    BollingerBands getLastBands();       // Déclare la méthode (l'implémentation sera dans le .cpp)

    // --- Destructeur (optionnel mais bonne pratique) ---
    // On peut en déclarer un virtuel si on prévoit d'hériter de cette classe
    // virtual ~BollingerBot() = default;

private:
    // --- Membres de configuration ---
    int period;
    double k;

    // --- Membres d'état ---
    std::vector<double> priceHistory;
    PositionState currentState;
    double entryPrice;

    // --- Fonctions utilitaires privées ---
    // Déclare les méthodes privées (l'implémentation sera dans le .cpp)
    double calculateSMA(const std::vector<double>& data);
    double calculateStdDev(const std::vector<double>& data, double sma);
    BollingerBands calculateBands();
};

#endif // BOLLINGER_BOT_H