#!/bin/bash

# Script pour lancer le client avec l'adresse IP locale et un port spécifié.

# --- Configuration ---
SERVER_PORT=4433 # Le port sur lequel ton serveur écoute
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
        echo "Assure-toi d'avoir compilé le client avec 'make'." # 'make Cli' ou 'make' tout court selon ta cible par défaut
        exit 1
    fi
fi

# *** Modifie les variables ci-dessous pour qu'elles correspondent à la source réelle de l'ID et du Token dans ton script ***
# Par exemple, si ton script prend l'ID et le Token en 1er et 2ème argument :
# CLIENT_ID="$1"
# CLIENT_TOKEN="$2"
# Ou si tu les lis depuis un fichier...

# Pour l'exemple, utilisons des valeurs fixes (à remplacer par ta logique réelle)
CLIENT_ID_TO_USE="Client1" # <<< Remplacer par la variable/source réelle de l'ID
CLIENT_TOKEN_TO_USE="Token1"   # <<< Remplacer par la variable/source réelle du Token


echo "Lancement du client : $CLIENT_EXEC $LOCAL_IP $SERVER_PORT $CLIENT_ID_TO_USE $CLIENT_TOKEN_TO_USE"
# Exécuter le client avec TOUS les arguments attendus (5 au total)
"$CLIENT_EXEC" "$LOCAL_IP" "$SERVER_PORT" "$CLIENT_ID_TO_USE" "$CLIENT_TOKEN_TO_USE" # <<< LIGNE MODIFIÉE


# Le script se termine ici lorsque le programme client se ferme.