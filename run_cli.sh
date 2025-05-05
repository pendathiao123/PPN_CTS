#!/bin/bash

# Script pour lancer le client avec l'adresse IP locale et un port spécifié.

# --- Configuration ---
SERVER_PORT=4433 
# --- Fin Configuration ---

# --- Déterminer l'adresse IP locale ---
LOCAL_IP=$(hostname -I | awk '{print $1}')

if [ -z "$LOCAL_IP" ]; then
    echo "Avertissement : Impossible de trouver l'adresse IP locale non-loopback automatiquement."
    echo "Utilisation de l'adresse localhost (127.0.0.1)."
    LOCAL_IP="127.0.0.1"
else
    echo "Adresse IP locale détectée : $LOCAL_IP"
fi

# --- Lancer le client ---
CLIENT_EXEC="./Test_Cli"

if [ ! -f "$CLIENT_EXEC" ]; then
    CLIENT_EXEC="./build/Test_Cli"
    if [ ! -f "$CLIENT_EXEC" ]; then
        echo "Erreur : L'exécutable client '$CLIENT_EXEC' n'a pas été trouvé."
        echo "Assure-toi d'avoir compilé le client avec 'make'." # 'make Cli' ou 'make' tout court selon la cible par défaut
        exit 1
    fi
fi

CLIENT_ID_TO_USE="Client1" 
CLIENT_TOKEN_TO_USE="Token1"   

echo "Lancement du client : $CLIENT_EXEC $LOCAL_IP $SERVER_PORT $CLIENT_ID_TO_USE $CLIENT_TOKEN_TO_USE"

"$CLIENT_EXEC" "$LOCAL_IP" "$SERVER_PORT" "$CLIENT_ID_TO_USE" "$CLIENT_TOKEN_TO_USE" 

echo "Lancement du client : $CLIENT_EXEC $LOCAL_IP $SERVER_PORT $CLIENT_ID_TO_USE $CLIENT_TOKEN_TO_USE"

"$CLIENT_EXEC" "$LOCAL_IP" "$SERVER_PORT" "$CLIENT_ID_TO_USE" "$CLIENT_TOKEN_TO_USE" 

