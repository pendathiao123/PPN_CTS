#!/bin/bash
#
# Permet de lancer Grafana et Prometheus + de les lier ensemble et d'afficher les datas
#
# Vérification des droits d'exécution
if [ "$EUID" -ne 0 ]; then
  echo "Ce script nécessite les droits d'administrateur. Veuillez l'exécuter avec sudo."
  exit 1
fi

# Paramètres de l'installation existante + token api pour récupérer le dashboard existant
GRAFANA_URL="http://localhost:3000"
GRAFANA_API_TOKEN="$GRAFANA_TOKEN"

# Recherche d'une installation de Prometheus sur l'ordinateur
PROMETHEUS_PATHS=(
    "/usr/bin/prometheus"
    "/usr/local/bin/prometheus" 
    "/opt/prometheus/prometheus"
    "/home/$SUDO_USER/prometheus/prometheus"
)

PROMETHEUS_PATH=""
for path in "${PROMETHEUS_PATHS[@]}"; do
    if [ -f "$path" ]; then
        PROMETHEUS_PATH="$path"
        PROMETHEUS_DIR=$(dirname "$PROMETHEUS_PATH")
        echo "Installation de Prometheus trouvée à: $PROMETHEUS_PATH"
        break
    fi
done

if [ -z "$PROMETHEUS_PATH" ]; then
    # recherche avec la commande which
    WHICH_PROMETHEUS=$(which prometheus 2>/dev/null)
    if [ -n "$WHICH_PROMETHEUS" ]; then
        PROMETHEUS_PATH="$WHICH_PROMETHEUS"
        PROMETHEUS_DIR=$(dirname "$PROMETHEUS_PATH")
        echo "Prometheus trouvé avec which: $PROMETHEUS_PATH"
    else
        echo "Prometheus n'est pas installé. Veuillez installer Prometheus."
        exit 1
    fi
fi

PROMETHEUS_CONFIG_PATHS=(
    "PPN_CTS/src/config.yml" #config du fichier de prometheus lié au projet
)

PROMETHEUS_CONFIG=""
for path in "${PROMETHEUS_CONFIG_PATHS[@]}"; do
    if [ -f "$path" ]; then
        PROMETHEUS_CONFIG="$path"
        echo "Configuration Prometheus trouvée à: $PROMETHEUS_CONFIG" #Verif
        break
    fi
done


# Copie du fichier de configuration Prometheus
echo "Configuration de Prometheus..."
cat > $PROMETHEUS_CONFIG << EOF
scrape_configs:
  - job_name: 'crypto-server'
    static_configs:
      - targets: ['127.0.0.1:8080']
EOF
echo "Configuration de Prometheus terminée."

# Vérification si Grafana est déjà en cours d'exécution
GRAFANA_RUNNING=false
if curl -s $GRAFANA_URL > /dev/null; then
    GRAFANA_RUNNING=true
    echo "Grafana est déjà en cours d'exécution à l'adresse $GRAFANA_URL"
else
    # Recherche de Grafana sur le système
    GRAFANA_SERVER_PATHS=(
        "/usr/sbin/grafana-server"
        "/usr/local/bin/grafana-server"
        "/opt/grafana/bin/grafana-server"
        "/usr/share/grafana/bin/grafana-server"
        "/home/$SUDO_USER/grafana/bin/grafana-server"
    )
    
    GRAFANA_SERVER_PATH=""
    for path in "${GRAFANA_SERVER_PATHS[@]}"; do
        if [ -f "$path" ]; then
            GRAFANA_SERVER_PATH="$path"
            GRAFANA_DIR=$(dirname "$(dirname "$GRAFANA_SERVER_PATH")")
            echo "Installation de Grafana trouvée à: $GRAFANA_SERVER_PATH"
            break
        fi
    done
    
    if [ -z "$GRAFANA_SERVER_PATH" ]; then
        # Recherche avec la commande which
        WHICH_GRAFANA=$(which grafana-server 2>/dev/null)
        if [ -n "$WHICH_GRAFANA" ]; then
            GRAFANA_SERVER_PATH="$WHICH_GRAFANA"
            GRAFANA_DIR=$(dirname "$(dirname "$GRAFANA_SERVER_PATH")")
            echo "Grafana trouvé avec which: $GRAFANA_SERVER_PATH"
        else
            echo "Grafana n'est pas installé. Veuillez installer Grafana."
            exit 1
        fi
    fi
fi

PROMETHEUS_PORT=9090

# Fonction pour vérifier si un port est libre
port_is_free() {
    if ! lsof -i:"$1" >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Vérifier si le port de Prometheus est libre
if ! port_is_free $PROMETHEUS_PORT; then
    lsof -i :9090
    echo "Le port $PROMETHEUS_PORT est déjà utilisé. Veuillez libérer ce port pour Prometheus; essayez sudo kill <PID> si le programme n'est pas important."
    exit 1
fi

# Démarrage de Prometheus en background
echo "Démarrage de Prometheus..."
$PROMETHEUS_PATH --config.file=$PROMETHEUS_CONFIG --web.listen-address=:$PROMETHEUS_PORT &
PROMETHEUS_PID=$!
echo "Prometheus démarré avec PID: $PROMETHEUS_PID"

# Attendre que Prometheus démarre complètement
sleep 2

# Vérifier que Prometheus est bien démarré
if ! ps -p $PROMETHEUS_PID > /dev/null; then
    echo "Erreur: Prometheus n'a pas démarré correctement."
    exit 1
fi

# Si Grafana n'est pas en cours d'exécution, le démarrer
if [ "$GRAFANA_RUNNING" = false ]; then
    echo "Démarrage de Grafana..."
    
    # Recherche du fichier de configuration de Grafana
    GRAFANA_CONFIG_PATHS=(
        "$GRAFANA_DIR/conf/defaults.ini"
        "$GRAFANA_DIR/conf/grafana.ini"
        "/etc/grafana/grafana.ini"
    )
    
    GRAFANA_CONFIG=""
    for path in "${GRAFANA_CONFIG_PATHS[@]}"; do
        if [ -f "$path" ]; then
            GRAFANA_CONFIG="$path"
            echo "Configuration Grafana trouvée à: $GRAFANA_CONFIG"
            break
        fi
    done
    
    if [ -z "$GRAFANA_CONFIG" ]; then
        echo "Aucun fichier de configuration Grafana trouvé. Utilisation des paramètres par défaut."
        $GRAFANA_SERVER_PATH &
    else
        $GRAFANA_SERVER_PATH --config=$GRAFANA_CONFIG &
    fi
    
    GRAFANA_PID=$!
    echo "Grafana démarré avec PID: $GRAFANA_PID"

    # Attendre que Grafana démarre complètement
    echo "Attente du démarrage de Grafana..."
    MAX_ATTEMPTS=30
    ATTEMPTS=0
    while ! curl -s $GRAFANA_URL > /dev/null && [ $ATTEMPTS -lt $MAX_ATTEMPTS ]; do
        sleep 1
        ((ATTEMPTS++))
    done

    if [ $ATTEMPTS -eq $MAX_ATTEMPTS ]; then
        echo "Erreur: Grafana n'a pas démarré correctement après $MAX_ATTEMPTS secondes."
        kill $PROMETHEUS_PID
        exit 1
    fi

    echo "Grafana est maintenant en cours d'exécution."
    
    # Création d'un fichier pour stocker le PID de Grafana
    echo "$GRAFANA_PID" > $WORK_DIR/grafana.pid
fi

# Création d'un fichier pour stocker le PID de Prometheus
echo "$PROMETHEUS_PID" > $WORK_DIR/prometheus.pid

# Configuration automatique de la source de données Prometheus dans Grafana via l'API
echo "Configuration de la source de données Prometheus dans Grafana..."

# Vérification si la source de données Prometheus existe déjà
DATASOURCE_EXISTS=$(curl -s -H "Authorization: Bearer $GRAFANA_API_TOKEN" \
    "$GRAFANA_URL/api/datasources/name/Prometheus")

if [[ $DATASOURCE_EXISTS == *"id"* ]]; then
    echo "La source de données Prometheus existe déjà dans Grafana."
else
    # Configuration de la source de données Prometheus
    curl -s -X POST -H "Content-Type: application/json" \
         -H "Authorization: Bearer $GRAFANA_API_TOKEN" \
         -d '{
            "name": "Prometheus",
            "type": "prometheus",
            "url": "http://localhost:9090",
            "access": "proxy",
            "isDefault": true
         }' \
         "$GRAFANA_URL/api/datasources" > /dev/null

    echo "Source de données Prometheus ajoutée à Grafana."
fi

echo "======================================================"
echo "Monitoring system started successfully!"
echo "======================================================"
echo "Prometheus est accessible à l'adresse: http://localhost:$PROMETHEUS_PORT"
echo "Grafana est accessible à l'adresse: $GRAFANA_URL"
echo "======================================================"
echo "Vous pouvez maintenant utiliser votre dashboard existant dans Grafana"
echo "======================================================"

# Garder le script actif pour maintenir les services en fonctionnement
if [ "$GRAFANA_RUNNING" = true ]; then
    echo "Appuyez sur CTRL+C pour arrêter Prometheus..."
    wait $PROMETHEUS_PID
else
    echo "Appuyez sur CTRL+C pour arrêter les services..."
    wait $PROMETHEUS_PID $GRAFANA_PID
fi