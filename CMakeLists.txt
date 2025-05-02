# Dans CMakeLists.txt à la racine du projet

# Spécifie la version minimale de CMake requise
# Augmentée légèrement si les fonctionnalités filesystem, atomic, thread, etc. sont utilisées et nécessitent une version plus récente pour une détection complète.
# 3.12 est un bon minimum pour C++17, mais 3.25 que vous avez est parfait.
cmake_minimum_required(VERSION 3.25)

# Définit le nom du projet
project(PPN_CTS CXX) # Indique que c'est un projet C++

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

# Recherche de nlohmann/json (généralement un header-only, on suppose qu'il est dans un include_directory)
# Si nlohmann/json est dans un répertoire spécifique (ex: dependencies/json), ajoute son répertoire à include_directories.
# message(STATUS "Assuming nlohmann/json is available via include path.")


# --- Définition des répertoires sources et headers ---

# Répertoire contenant les fichiers .cpp
# Basé sur la structure : PPN_CTS/src/code/
set(CODE_DIR ${CMAKE_SOURCE_DIR}/src/code)

# Répertoire contenant les fichiers .h
# Basé sur la structure : PPN_CTS/src/headers/
set(HEADERS_DIR ${CMAKE_SOURCE_DIR}/src/headers)


# Note : Vérifie bien que les chemins ci-dessus correspondent à l'emplacement réel de tes fichiers.


# --- Liste de TOUS les fichiers source (.cpp) par exécutable ---

# Sources nécessaires pour l'exécutable serveur (Test_Serv).
# Doit inclure TOUS les .cpp dont le serveur a besoin pour compiler et lier.
set(SERVER_SRC
    ${CODE_DIR}/Main_Serv.cpp
    ${CODE_DIR}/Server.cpp
    ${CODE_DIR}/ServerConnection.cpp   
    ${CODE_DIR}/ClientAuthenticator.cpp
    ${CODE_DIR}/ClientSession.cpp
    ${CODE_DIR}/Transaction.cpp
    ${CODE_DIR}/TransactionQueue.cpp
    ${CODE_DIR}/Global.cpp
    ${CODE_DIR}/Bot.cpp
    ${CODE_DIR}/Logger.cpp
    ${CODE_DIR}/Wallet.cpp
    ${CODE_DIR}/Utils.cpp             
    # Vérifie si d'autres .cpp sont nécessaires au serveur
)

# Sources nécessaires pour l'exécutable client (Test_Cli).
# Doit inclure TOUS les .cpp dont le client a besoin.
set(CLIENT_SRC
    ${CODE_DIR}/Main_Cli.cpp
    ${CODE_DIR}/ClientInitiator.cpp    
    ${CODE_DIR}/Logger.cpp
    ${CODE_DIR}/ServerConnection.cpp
    # ${CODE_DIR}/Utils.cpp # Le client inclut Utils.h mais n'a pas besoin des implémentations .cpp
    ${CODE_DIR}/Transaction.cpp # Inclure si le client utilise les méthodes de Transaction (peu probable)
    ${CODE_DIR}/Wallet.cpp # Inclure si le client utilise la classe Wallet (peu probable)
    # Vérifie si d'autres .cpp sont nécessaires au client
)

set(BENCHMARK_SRC
    ${CODE_DIR}/Benchmark.cpp
    ${CODE_DIR}/ClientInitiator.cpp
    ${CODE_DIR}/Logger.cpp
    ${CODE_DIR}/Transaction.cpp
    ${CODE_DIR}/ServerConnection.cpp
)


# --- Configuration de compilation ---

# Ajouter les répertoires d'en-têtes
include_directories(${HEADERS_DIR}) # Ton répertoire d'en-têtes (où sont Client.h, Server.h, etc.)
include_directories(${CURL_INCLUDE_DIRS}) # En-têtes CURL trouvés par find_package
include_directories(${OpenSSL_INCLUDE_DIRS}) # En-têtes OpenSSL trouvés par find_package
# include_directories(${CMAKE_SOURCE_DIR}/dependencies/json) # Exemple si nlohmann/json est téléchargé localement

# OPTIONS POUR LA CONSTRUCTION
set(CMAKE_BUILD_TYPE Debug)  # Ou Release

# Options de compilation standard
add_compile_options(-std=c++17 -Wall -Wextra -pedantic) # Options de warning strictes

# Ajouter l'option pour générer des symboles de débogage si en mode Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -ggdb) # Flags de débogage pour GDB
endif()

# Option pour supprimer les warnings sur les fonctions obsolètes (comme ERR_load_SSL_strings)
# C'est un workaround. La bonne pratique serait de remplacer les appels obsolètes.
add_compile_options(-Wno-deprecated-declarations)


# --- Création des exécutables ---

# Création de l'exécutable serveur
add_executable(Test_Serv ${SERVER_SRC})

# Création de l'exécutable client
add_executable(Test_Cli ${CLIENT_SRC})

# Création de l'exécutable benchmark
add_executable(Test_Benchmark ${BENCHMARK_SRC})

# --- Lier les bibliothèques aux exécutables ---

# Lier les bibliothèques nécessaires au serveur
target_link_libraries(Test_Serv
    OpenSSL::SSL    # Lie spécifiquement à la bibliothèque OpenSSL SSL
    OpenSSL::Crypto # Lie spécifiquement à la bibliothèque OpenSSL Crypto
    ${CURL_LIBRARIES}     # Lie la bibliothèque CURL trouvée
    ${CMAKE_THREAD_LIBS_INIT} # Lie les bibliothèques nécessaires pour std::thread
    # Ajoute d'autres bibliothèques si nécessaire (ex: nlohmann_json si tu l'installes via ton système)
    # Si tu utilises std::filesystem, tu pourrais avoir besoin de lier à une bibliothèque système comme -lstdc++fs ou similaires,
    # mais C++17 et un compilateur récent l'incluent souvent dans -lstdc++.
)

# Lier les bibliothèques nécessaires au client
target_link_libraries(Test_Cli
    OpenSSL::SSL # Le client utilise OpenSSL pour la connexion SSL
    OpenSSL::Crypto # Le client utilise OpenSSL pour la connexion SSL
    ${CURL_LIBRARIES} # Si ton client utilise Curl (probablement non)
    ${CMAKE_THREAD_LIBS_INIT} # Si ton client utilise des threads (probablement non pour un client simple)
    # Si tu utilises std::filesystem dans le client, tu pourrais avoir besoin de lier ici aussi.
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
    COMMAND $<TARGET_FILE:Test_Serv>
    DEPENDS Test_Serv
    COMMENT "Running Server executable"
)

# Cible pour exécuter le client après la construction
add_custom_target(Cli
    COMMAND $<TARGET_FILE:Test_Cli>
    DEPENDS Test_Cli
    COMMENT "Running Client executable"
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
)

# Cible pour exécuter le client sous GDB
add_custom_target(gdb_client
    COMMAND gdb $<TARGET_FILE:Test_Cli>
    DEPENDS Test_Cli
    COMMENT "Running Client executable under GDB"
)