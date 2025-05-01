#!/bin/bash

# Script pour trouver le PID du serveur Test_Serv et l'arrêter.

# --- Configuration ---
SERVER_PROCESS_NAME="Test_Serv" # Nom de l'exécutable du serveur
# --- Fin Configuration ---

echo "Recherche du processus serveur '$SERVER_PROCESS_NAME'..."

# Utiliser 'ps aux' (ou 'ps -ef' selon le système) pour lister les processus,
# puis 'grep' pour filtrer ceux dont le nom correspond.
# La syntaxe '[T]est_Serv' (avec la première lettre entre crochets) est une astuce
# pour éviter que le 'grep' lui-même n'apparaisse dans les résultats.
# 'awk '{print $2}'" extrait le PID (qui est généralement la 2ème colonne).
SERVER_PID=$(ps aux | grep "[${SERVER_PROCESS_NAME:0:1}]${SERVER_PROCESS_NAME:1}" | awk '{print $2}')

# Vérifier si un PID a été trouvé.
if [ -z "$SERVER_PID" ]; then
    echo "Processus serveur '$SERVER_PROCESS_NAME' non trouvé. Il n'est peut-être pas en cours d'exécution."
else
    echo "Processus serveur '$SERVER_PROCESS_NAME' trouvé avec le PID : $SERVER_PID"
    
    # --- Tenter d'arrêter le processus gracieusement d'abord (SIGTERM) ---
    # SIGTERM (signal par défaut de kill) demande au processus de s'arrêter proprement.
    # C'est préférable car cela laisse le programme gérer sa propre sortie (cleanup, sauvegarde, etc.).
    echo "Tentative d'arrêt gracieux du serveur (signal SIGTERM)..."
    kill "$SERVER_PID"
    
    # --- Optionnel : Attendre un peu et vérifier si le processus s'est arrêté ---
    # On attend quelques secondes pour laisser au serveur le temps de s'arrêter.
    sleep 3 # Attendre 3 secondes
    
    # Vérifier si le processus avec ce PID existe toujours.
    # 'ps -p <PID>' vérifie l'existence du processus. '> /dev/null' cache sa sortie.
    if ps -p "$SERVER_PID" > /dev/null; then
        # Si le processus existe toujours (il n'a pas répondu à SIGTERM)
        echo "Le serveur ne s'est pas arrêté après SIGTERM. Envoi du signal KILL (SIGKILL)..."
        # SIGKILL (kill -9) force l'arrêt immédiat du processus. À utiliser en dernier recours.
        kill -9 "$SERVER_PID"
        echo "Signal KILL envoyé."
    else
        # Le processus s'est arrêté dans le délai après SIGTERM.
        echo "Le serveur s'est arrêté avec succès."
    fi
fi

exit 0 # Quitter le script avec succès