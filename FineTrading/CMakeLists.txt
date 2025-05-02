cmake_minimum_required(VERSION 3.25)
project(FineTrading)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Assurez-vous que la version de libcurl du système est utilisée
set(CURL_LIBRARY /usr/lib/x86_64-linux-gnu/libcurl.so)
set(CURL_INCLUDE_DIR /usr/include)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)


# Active SSL (si tu l'as installé)
set(CPR_ENABLE_SSL ON CACHE BOOL "Enable SSL support for cpr")

# Ajouter les fichiers source
add_executable(FineTrading
    src/main.cpp
    src/Portfolio.cpp
    src/CryptoData.cpp
    src/Strategy.cpp
    src/Bollinger.cpp
)

# Ajoute cpr en tant que sous-dossier
add_subdirectory(cpr)

# Include headers
include_directories(headers)
include_directories(cpr/include)

# Lier cpr et curl à ton exécutable
target_link_libraries(FineTrading PRIVATE cpr ${CURL_LIBRARIES})
target_link_libraries(FineTrading PRIVATE nlohmann_json::nlohmann_json)

