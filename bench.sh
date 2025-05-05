#!/bin/bash

# --- Configuration ---
SERVER_IP="127.0.0.1"
SERVER_PORT=4433
START_CONNECTIONS=10
MAX_CONNECTIONS=100
STEP=10
AUTH_MESSAGE="ID:Client1,TOKEN:Token1"

# --- Vérifications préliminaires ---
if ! command -v openssl &> /dev/null; then
    echo "Erreur : 'openssl' n'est pas installé."
    exit 1
fi

# Vérifie si l'hôte et le port sont accessibles
echo | openssl s_client -connect "$SERVER_IP:$SERVER_PORT" -quiet > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "Erreur : Impossible de se connecter à $SERVER_IP:$SERVER_PORT. Vérifiez que le serveur est en cours d'exécution."
    exit 1
fi

# --- Fonction de test ---
function test_connections() {
    local num_connections=$1
    local success_count=0
    local fail_count=0
    local pids=()

    echo "Test avec $num_connections connexions simultanées..."

    # Démarre les connexions en parallèle
    for ((i = 0; i < num_connections; i++)); do
        {
            echo "$AUTH_MESSAGE" | openssl s_client -connect "$SERVER_IP:$SERVER_PORT" -quiet > /dev/null 2>&1
            if [ $? -eq 0 ]; then
                ((success_count++))
            else
                ((fail_count++))
            fi
        } &
        pids+=($!) # Stocke le PID du processus en arrière-plan
    done

    # Attendre que tous les processus parallèles se terminent
    for pid in "${pids[@]}"; do
        wait "$pid" 2>/dev/null
    done

    echo "Connexions réussies : $success_count"
    echo "Connexions échouées : $fail_count"
    return $success_count
}

# --- Boucle principale ---
current_connections=$START_CONNECTIONS
while (( current_connections <= MAX_CONNECTIONS )); do
    test_connections $current_connections
    result=$?

    if (( result < current_connections )); then
        echo "Le serveur a atteint sa limite avec $((current_connections - STEP)) connexions simultanées."
        break
    fi

    current_connections=$((current_connections + STEP))
done

echo "Benchmark terminé."