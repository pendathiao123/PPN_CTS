cmake_minimum_required(VERSION 3.10)
project(PPN_CTS)

# Répertoires de sources et d'en-têtes relatifs à PPN/src/code
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src/code)    # Répertoire contenant les .cpp
set(INC_DIR ${CMAKE_SOURCE_DIR}/src/headers) # Répertoire contenant les .h

# Liste des fichiers source pour le serveur et le client
set(SERVER_SRC 
    ${SRC_DIR}/Server.cpp
    ${SRC_DIR}/Transaction.cpp
    ${SRC_DIR}/Main_Serv.cpp
    ${SRC_DIR}/Crypto.cpp
    ${SRC_DIR}/SRD-BTC.cpp
    ${SRC_DIR}/global.cpp
    ${SRC_DIR}/bot.cpp
    #${SRC_DIR}/Client.cpp
)
set(CLIENT_SRC 
    ${SRC_DIR}/Client.cpp
    ${SRC_DIR}/Transaction.cpp
    ${SRC_DIR}/Main_Cli.cpp
    ${SRC_DIR}/global.cpp
    ${SRC_DIR}/Crypto.cpp
    ${SRC_DIR}/bot.cpp
)

# Créer un répertoire de sortie pour les exécutables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/exec)

# Création des exécutables
add_executable(Test_Serv ${SERVER_SRC})
add_executable(Test_Cli ${CLIENT_SRC})

# Inclure les répertoires des headers
include_directories(${INC_DIR})

# Options de compilation
add_compile_options(-std=c++17 -Wall)

# Lier les bibliothèques OpenSSL
target_link_libraries(Test_Serv ssl crypto)
target_link_libraries(Test_Cli ssl crypto)

# Ajouter des règles pour exécuter les exécutables après leur construction
add_custom_target(Serv
    COMMAND ${CMAKE_BINARY_DIR}/exec/Test_Serv
    DEPENDS Test_Serv 
) 

add_custom_target(Cli
COMMAND ${CMAKE_BINARY_DIR}/exec/Test_Cli
DEPENDS Test_Cli
) 

