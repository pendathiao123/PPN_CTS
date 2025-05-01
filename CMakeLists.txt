# Dans CMakeLists.txt à la racine du projet

# Spécifie la version minimale de CMake requise
cmake_minimum_required(VERSION 3.25)

# Définit le nom du projet
project(PPN_CTS CXX) # Indique que c'est un projet C++

# Active les fonctionnalités étendues (pour des choses comme file(GLOB))
# set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true) # Obsolète, pas nécessaire ici

# --- Recherche des dépendances externes ---

# Recherche de CURL (REQUIRED signifie que CMake échouera si CURL n'est pas trouvé)
find_package(CURL REQUIRED)

if(CURL_FOUND)
    message(STATUS "CURL library found at ${CURL_LIBRARIES}")
    message(STATUS "CURL include dir: ${CURL_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "CURL not found! Please install libcurl development files.")
endif()

# Recherche d'OpenSSL (REQUIRED)
# Cherche les composants SSL et Crypto
find_package(OpenSSL REQUIRED)

if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found")
    message(STATUS "  Include dirs: ${OpenSSL_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${OpenSSL_LIBRARIES}")
else()
    message(FATAL_ERROR "OpenSSL not found! Please install libssl-dev and libcrypto-dev.")
endif()

# Recherche de nlohmann/json (généralement un header-only, donc on ne le 'find_package' pas classiquement)
# On suppose que tu l'as téléchargé et mis dans un répertoire inclus, ou installé via un gestionnaire.
# S'il est dans un répertoire spécifique (ex: dependencies/json), ajoute son répertoire à include_directories.
# message(STATUS "Assuming nlohmann/json is available via include path.")


# --- Définition des répertoires sources et headers (CORRIGÉ) ---

# Répertoire racine des sources (là où se trouve src/ et headers/ si ta structure est PPN_CTS/src/...)
# Si src/ et headers/ sont directement sous PPN_CTS, utilise CMAKE_SOURCE_DIR directement.
# set(SRC_ROOT ${CMAKE_SOURCE_DIR}) # Si src et headers sont à la racine PPN_CTS
# Sinon, si src/ et headers/ sont sous un répertoire comme 'src' sous PPN_CTS:
set(SRC_ROOT ${CMAKE_SOURCE_DIR}/src) # Si src et headers sont sous PPN_CTS/src/

# Répertoire contenant les fichiers .cpp (CORRIGÉ)
# Basé sur ta structure originale : PPN_CTS/src/code/
set(CODE_DIR ${SRC_ROOT}/code)

# Répertoire contenant les fichiers .h (CORRIGÉ)
# Basé sur ta structure originale : PPN_CTS/src/headers/
set(HEADERS_DIR ${SRC_ROOT}/headers)


# Note : Vérifie bien que les chemins ci-dessus (${SRC_ROOT}, ${CODE_DIR}, ${HEADERS_DIR})
# correspondent exactement à l'emplacement de tes fichiers sur ton système
# par rapport au répertoire où se trouve ton CMakeLists.txt racine.
# Par exemple, si tes fichiers .cpp sont dans PPN_CTS/src/code/ et tes .h dans PPN_CTS/src/headers/.
# Si tous les .cpp sont dans PPN_CTS/src/ directement, utilise : set(CODE_DIR ${SRC_ROOT})
# --- Liste des fichiers source pour chaque exécutable ---

# Fichiers source spécifiques au serveur
set(SERVER_SPECIFIC_SRC
    ${CODE_DIR}/Main_Serv.cpp
    ${CODE_DIR}/Server.cpp
    ${CODE_DIR}/BotSession.cpp # BotSession est spécifique au serveur
    # Global.cpp, TransactionQueue.cpp, Bot.cpp, Transaction.cpp, Client.cpp, Logger.cpp sont partagés
    # et seront listés ci-dessous
)

# Fichiers source spécifiques au client
set(CLIENT_SPECIFIC_SRC
    ${CODE_DIR}/Main_Cli.cpp
    # Client.cpp est partagé
    # Transaction.cpp (peut-être nécessaire pour lire l'historique côté client si tu le fais)
    # Logger.cpp est partagé
)

# Fichiers source partagés entre le serveur et le client (ou nécessaires aux deux)
set(SHARED_SRC
    ${CODE_DIR}/Client.cpp
    ${CODE_DIR}/Transaction.cpp # Transaction.cpp est nécessaire pour logTransactionToCSV (côté server) ou readTransaction (côté client si implémenté)
    ${CODE_DIR}/TransactionQueue.cpp # TransactionQueue est principalement côté serveur (extern)
    ${CODE_DIR}/Global.cpp # Global est principalement côté serveur (thread de prix) mais getPrice/getPreviousPrice sont utilisés par le Bot (côté server)
    ${CODE_DIR}/Bot.cpp # Bot est uniquement côté serveur
    ${CODE_DIR}/Logger.cpp # Logger est utilisé des deux côtés
)

# Compiler l'ensemble des sources pour le serveur
set(SERVER_ALL_SRC
    ${SERVER_SPECIFIC_SRC}
    ${SHARED_SRC}
)

# Compiler seulement les sources nécessaires pour le client
# Le client n'a PAS besoin de Server.cpp, BotSession.cpp, TransactionQueue.cpp, Global.cpp, Bot.cpp
set(CLIENT_ALL_SRC
    ${CLIENT_SPECIFIC_SRC}
    ${CODE_DIR}/Client.cpp # Client.cpp est utilisé par main_Cli.cpp
    ${CODE_DIR}/Transaction.cpp # Si tu utilises readTransaction ou la structure Transaction côté client
    ${CODE_DIR}/Logger.cpp # Si tu utilises le Logger côté client
    # Assure-toi que les .h inclus dans main_Cli.cpp et Client.cpp sont corrects et suffisants.
    # Ex: main_Cli.cpp inclut Bot.h, ce qui est étrange si Bot n'est pas utilisé côté client.
    # Corrigeons main_Cli.cpp pour qu'il n'inclue que ce dont il a besoin.
)

set(BENCHMARK_SRC
    ${CODE_DIR}/Benchmark.cpp
    ${CODE_DIR}/Client.cpp
    ${CODE_DIR}/Logger.cpp
    ${CODE_DIR}/Transaction.cpp
)


# --- Configuration de compilation ---

# Ajouter les répertoires d'en-têtes
include_directories(${HEADERS_DIR}) # Ton répertoire d'en-têtes
include_directories(${CURL_INCLUDE_DIRS}) # En-têtes CURL trouvés
include_directories(${OpenSSL_INCLUDE_DIRS}) # En-têtes OpenSSL trouvés
# include_directories(${CMAKE_SOURCE_DIR}/dependencies/json) # Exemple si nlohmann/json est là
# include_directories(${SRC_ROOT}/code) # Inclure le répertoire des .cpp si certains headers y sont (rare)
# include_directories(${CMAKE_SOURCE_DIR}/src/code) # Autre façon d'écrire


# OPTIONS POUR LE DÉBOGAGE
set(CMAKE_BUILD_TYPE Debug)  # Mode Debug pour gdb (symboles de débogage)
# set(CMAKE_BUILD_TYPE Release) # Mode Release pour optimisation

# Options de compilation standard
add_compile_options(-std=c++17 -Wall -Wextra -pedantic) # -Wextra et -pedantic ajoutent plus d'avertissements
# Ajouter l'option pour générer des symboles de débogage si en mode Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -ggdb) # -ggdb pour GDB
endif()


# Créer un répertoire de sortie pour les exécutables dans le répertoire build (par défaut)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Exemple pour mettre les binaires dans build/bin


# --- Création des exécutables ---

# Création de l'exécutable serveur
add_executable(Test_Serv ${SERVER_ALL_SRC})

# Création de l'exécutable client (sources corrigées)
add_executable(Test_Cli ${CLIENT_ALL_SRC})

# Création de l'exécutable benchmark
add_executable(Test_Benchmark ${BENCHMARK_SRC})

# --- Lier les bibliothèques aux exécutables ---

# Lier les bibliothèques nécessaires au serveur
target_link_libraries(Test_Serv
    OpenSSL::SSL    # Lie spécifiquement à la bibliothèque OpenSSL SSL
    OpenSSL::Crypto # Lie spécifiquement à la bibliothèque OpenSSL Crypto
    ${CURL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT} # Pour les threads C++11
    # Ajoute d'autres bibliothèques si nécessaire (ex: nlohmann_json si tu l'installes via ton système)
)

# Lier les bibliothèques nécessaires au client
target_link_libraries(Test_Cli
    OpenSSL::SSL    # Lie le client à la bibliothèque OpenSSL SSL
    OpenSSL::Crypto # Lie le client à la bibliothèque OpenSSL Crypto
    ${CURL_LIBRARIES} # Si ton client utilise Curl
    ${CMAKE_THREAD_LIBS_INIT} # Si ton client utilise des threads
)

# Lier les bibliothèques nécessaires au benchmark
target_link_libraries(Test_Benchmark
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CURL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# --- Cibles personnalisées pour exécuter ---

# Cible pour exécuter le serveur après la construction
add_custom_target(Serv
    COMMAND $<TARGET_FILE:Test_Serv> # Utilise le chemin correct de l'exécutable
    DEPENDS Test_Serv
    COMMENT "Running Server executable"
    # WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin # Si tu utilises un sous-répertoire pour les binaires
)

# Cible pour exécuter le client après la construction
add_custom_target(Cli
    COMMAND $<TARGET_FILE:Test_Cli> # Utilise le chemin correct de l'exécutable
    DEPENDS Test_Cli
    COMMENT "Running Client executable"
    # WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# --- Cible pour exécuter benchmark ---
add_custom_target(Benchmark
    COMMAND $<TARGET_FILE:Test_Benchmark>
    DEPENDS Test_Benchmark
    COMMENT "Running Benchmark executable"
)

# --- Cible de nettoyage ---

# Cible pour nettoyer le répertoire de build
add_custom_target(my_clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning all contents in the build directory (${CMAKE_BINARY_DIR})"
)

# --- Cibles pour le débogage avec GDB ---

# Cible pour exécuter le serveur sous GDB
add_custom_target(gdb_serveur
    COMMAND gdb $<TARGET_FILE:Test_Serv>
    DEPENDS Test_Serv
    COMMENT "Running Server executable under GDB"
    # WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Cible pour exécuter le client sous GDB
add_custom_target(gdb_client
    COMMAND gdb $<TARGET_FILE:Test_Cli>
    DEPENDS Test_Cli
    COMMENT "Running Client executable under GDB"
    # WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)