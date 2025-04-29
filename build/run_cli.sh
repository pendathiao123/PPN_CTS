#!/bin/bash

# Script pour lancer le client avec l'adresse IP locale et un port spécifié.

# --- Configuration ---
SERVER_PORT=4433 # Le port sur lequel ton serveur écoute
# --- Fin Configuration ---

# --- Déterminer l'adresse IP locale ---
# Cette commande tente de trouver les adresses IP non-loopback (pas 127.0.0.1)
# Elle est courante sur de nombreux systèmes Linux.
# On prend le premier résultat s'il y en a plusieurs.
LOCAL_IP=$(hostname -I | awk '{print $1}')

# Si la commande ci-dessus n'a rien retourné, on utilise l'adresse localhost (127.0.0.1)
if [ -z "$LOCAL_IP" ]; then
    echo "Avertissement : Impossible de trouver l'adresse IP locale non-loopback automatiquement."
    echo "Utilisation de l'adresse localhost (127.0.0.1)."
    LOCAL_IP="127.0.0.1"
else
    echo "Adresse IP locale détectée : $LOCAL_IP"
fi

# --- Lancer le client ---
# Assure-toi que ton exécutable client s'appelle bien Test_Cli
CLIENT_EXEC="./Test_Cli"

# Vérifier si l'exécutable existe (d'abord dans le répertoire courant, puis dans ./build/)
if [ ! -f "$CLIENT_EXEC" ]; then
    CLIENT_EXEC="./build/Test_Cli"
    if [ ! -f "$CLIENT_EXEC" ]; then
        echo "Erreur : L'exécutable client '$CLIENT_EXEC' n'a pas été trouvé."
        echo "Assure-toi d'avoir compilé le client avec 'make Cli'."
        exit 1 # Quitter le script avec une erreur
    fi
fi

echo "Lancement du client : $CLIENT_EXEC $LOCAL_IP $SERVER_PORT"
# Exécuter le client avec les arguments
"$CLIENT_EXEC" "$LOCAL_IP" "$SERVER_PORT"

# Le script se termine ici lorsque le programme client se ferme.