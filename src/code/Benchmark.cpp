#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <atomic>
#include <condition_variable>
#include <string> 
#include <fcntl.h>  // Pour fcntl
#include <signal.h> // Pour ignorer SIGPIPE signal

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h> 
#include <openssl/crypto.h> 
#include "../headers/Client.h"
#include "../headers/Logger.h"  
#include "../headers/OpenSSLDeleters.h"

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 4433;
constexpr int CONNECTION_TIMEOUT_MS = 1000;
constexpr int MAX_THREADS = 100;
constexpr int READ_TIMEOUT_MS = 2000;

std::mutex cout_mutex;  //mutex affichage
std::atomic<int> successful_connections(0);
std::atomic<int> failed_connections(0);
std::condition_variable cv;
std::mutex cv_mutex;

// Configure un socket non bloquant avec timeout
bool set_socket_non_blocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        LOG("Erreur: Impossible d'obtenir les flags du socket", "ERROR");
        return false;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG("Erreur: Impossible de définir le socket comme non bloquant", "ERROR");
        return false;
    }
    return true;
}

// Connexion avec timeout utilisant select()
bool connect_with_timeout(int sock, sockaddr_in* server_addr, int timeout_ms) {
    if (!set_socket_non_blocking(sock)) {
        return false;
    }
    int res = connect(sock, (sockaddr*)server_addr, sizeof(*server_addr));
    if (res < 0) {
        if (errno != EINPROGRESS) {
            return false;
        }
        // Configurer select pour attendre la connexion
        fd_set writefds, errorfds;
        FD_ZERO(&writefds);
        FD_ZERO(&errorfds);
        FD_SET(sock, &writefds);
        FD_SET(sock, &errorfds);

        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int select_res = select(sock + 1, NULL, &writefds, &errorfds, &timeout);
        
        if (select_res <= 0) {
            return false; // Timeout ou erreur
        }
        
        // Vérifier si la connexion a réussi
        if (FD_ISSET(sock, &errorfds)) {
            int error;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                return false;
            }
        }
    }
    // Remettre le socket en mode bloquant
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1 || fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        return false;
    }
    
    return true;
}

// Fonction exécutée par chaque thread pour tester une connexion
void connect_to_server(int id) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Créer un socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "\033[1;31mClient #" << id << ": Erreur création socket\033[0m\n";
        failed_connections++;
        return;
    }
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    auto connect_start = std::chrono::high_resolution_clock::now();
    bool connected = connect_with_timeout(sock, &server_addr, CONNECTION_TIMEOUT_MS);
    auto connect_end = std::chrono::high_resolution_clock::now();
    
    if (!connected) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "\033[1;31mClient #" << id << ": Échec connexion TCP\033[0m\n";
        close(sock);
        failed_connections++;
        return;
    }

    // Créer un objet SSL_CTX pour cette connexion
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "\033[1;31mClient #" << id << ": Erreur création SSL_CTX\033[0m\n";
        close(sock);
        failed_connections++;
        return;
    }

    // Créer un objet SSL en utilisant le contexte SSL_CTX
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "\033[1;31mClient #" << id << ": Échec création SSL\033[0m\n";
        SSL_CTX_free(ctx);
        close(sock);
        failed_connections++;
        return;
    }

    // Connecter le socket à SSL
    if (!SSL_set_fd(ssl, sock)) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "\033[1;31mClient #" << id << ": Erreur SSL_set_fd\033[0m\n";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        failed_connections++;
        return;
    }
    
    // Initier la connexion SSL
    int ssl_result = SSL_connect(ssl);
    if (ssl_result <= 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "\033[1;31mClient #" << id << ": Échec connexion SSL\033[0m\n";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        failed_connections++;
        return;
    }
    
    // Envoi d'une chaîne d'authentification fictive après une connexion SSL réussie, wallet aura donc le nom benchmark
    const char* auth_message = "ID:benchmark,TOKEN:abc123\n";
    int bytes_sent = SSL_write(ssl, auth_message, strlen(auth_message));
    if (bytes_sent <= 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);   //permet de pas mélanger tous les clients dans l'affichage 
        std::cerr << "Client #" << id << ": Échec envoi message d'authentification\n";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        failed_connections++;
    return;
    }
    // Connexion réussie
    successful_connections++;
    
    // Mesurer les latences, temps total entre envoi et réception de l'authorisation de connexion
    std::chrono::duration<double> connect_latency = connect_end - connect_start;
    std::chrono::duration<double> ssl_latency = std::chrono::high_resolution_clock::now() - connect_end;
    std::chrono::duration<double> total_latency = std::chrono::high_resolution_clock::now() - start_time;
    
    /* Affichage débug 
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Client #" << id << " | TCP: " << connect_latency.count() * 1000 << "ms"
                  << " | SSL: " << ssl_latency.count() * 1000 << "ms"
                  << " | Total: " << total_latency.count() * 1000 << "ms\n";
    }
    */
    // Fermer la connexion proprement
    SSL_shutdown(ssl);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);
}

void test_connections(int num_clients) {
    std::vector<std::thread> threads;
    successful_connections = 0;
    failed_connections = 0;
    
    /*On mesure le temps que prennent tous les threads a se connecter puis on va diviser le nombre de clients par le temps mesuré*/
    auto start = std::chrono::high_resolution_clock::now();
    
    // Créer les threads de test
    for (int i = 0; i < num_clients; ++i) {
        // Limite le nombre de threads simultanés, utile si on veut faire des mesures sur de la durée (1000 clients par paquet de 10) ou bien en brut (1000 clients d'un coup)
        if (threads.size() >= MAX_THREADS) {
            threads.front().join();
            threads.erase(threads.begin());
        }
        //fonction routine des threads
        threads.emplace_back(connect_to_server, i);
        
        //On fait une mini pause pour éviter une surchage serveur qui poserait problème sur SSL
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    
    double cps = num_clients / duration.count();    //connection per second
    
    // Afficher les résultats du benchmark [Connexions totales avec le nombre de réussites et d'erreurs + en %, CPS connections per second et temps moyen pour une connexion calculé]
    std::cout << "\n=== Résultat du benchmark pour " << num_clients << " clients ===\n";
    std::cout << "Total de connexions : " << num_clients << "\n";
    std::cout << "Connexions réussies : " << successful_connections.load() << " (" << (successful_connections.load() * 100.0 / num_clients) << "%)\n";
    std::cout << "Connexions échouées : " << failed_connections.load() << " (" << (failed_connections.load() * 100.0 / num_clients) << "%)\n";
    std::cout << "Durée : " << duration.count() << " s\n";
    std::cout << "Connexions par seconde (CPS) : " << cps << "\n";
    std::cout << "Temps moyen par connexion : " << (duration.count() * 1000 / num_clients) << " ms\n\n";    
}

int main() {
    //ignorer les erreurs dues au connexions/deconnexion trop rapides
    signal(SIGPIPE, SIG_IGN);
    // Initialiser OpenSSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    LOG("Démarrage des tests de benchmark", "INFO");
    
    // Exécuter les tests avec différentes charges
    LOG("Démarrage du benchmark avec 10 clients", "INFO");
    test_connections(10);

    LOG("Démarrage du benchmark avec 100 clients", "INFO");
    test_connections(100);
    
    LOG("Démarrage du benchmark avec 1000 clients", "INFO");
    test_connections(1000);

    LOG("Démarrage du benchmark avec 10000 clients", "INFO");
    test_connections(10000);

    LOG("Démarrage du benchmark avec 20000 clients", "INFO");
    test_connections(20000);

    return 0;
}