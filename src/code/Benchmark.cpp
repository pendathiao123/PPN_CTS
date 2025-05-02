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
        int auth_err = SSL_get_error(ssl, bytes_sent);
        std::lock_guard<std::mutex> lock(cout_mutex);   //permet de pas mélanger tous les clients dans l'affichage 
        std::cerr << "Client #" << id << ": auth_failed, error = " << auth_err << "\n";
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

/* Benchmark 1 : CPS; utilise la fonction connect_to_server pour lancer un certain nombre de clients par paquets, 
permet de tester la rapidité de connexion clients. On pourra calculer le CPS (connections par seconde), mais aussi la latence de connexion + connexion SSL.*/
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


/* Benchmark 2 : TPS; permet de mesurer le débit, on va envoyer un nombre important de transactions/requêtes avec un seul client 
pour déterminer si la gestion de requêtes est surchargée ou non. On pourra calculer le TPS (transactions par seconde).*/
void test_transactions(int nb_transactions) {
    int successful_transactions_loc = 0;
    int failed_transactions_loc = 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Erreur création socket pour la transaction\n";
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    bool connected = connect_with_timeout(sock, &server_addr, CONNECTION_TIMEOUT_MS);
    if (!connected) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Échec connexion TCP pour la transaction\n";
        close(sock);
        return;
    }

    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        std::cerr << "Erreur création SSL_CTX\n";
        close(sock);
        return;
    }

    SSL* ssl = SSL_new(ctx);
    if (!ssl || !SSL_set_fd(ssl, sock)) {
        std::cerr << "Erreur création SSL ou set_fd\n";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return;
    }

    if (SSL_connect(ssl) <= 0) {
        std::cerr << "Échec connexion SSL\n";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return;
    }

    const char* auth_message = "ID:benchmark,TOKEN:abc123\n";
    int auth_sent = SSL_write(ssl, auth_message, strlen(auth_message));
    if (auth_sent <= 0) {
        std::cerr << "Échec envoi message d'authentification\n";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return;
    }

    auto start_tps = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < nb_transactions; ++i) {
        const char* transaction_message = "SHOW WALLET";
        int bytes_sent = SSL_write(ssl, transaction_message, strlen(transaction_message));
        if (bytes_sent <= 0) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "Échec envoi transaction #" << i << "\n";
            failed_transactions_loc++;
            continue;
        }

        // Lecture réponse éventuelle si nécessaire
        char buffer[4096];
        int bytes_read = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "Échec lecture réponse transaction #" << i << "\n";
            failed_transactions_loc++;
            continue;
        }

        successful_transactions_loc++;
    }

    auto end_tps = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_tps - start_tps;
    double tps = successful_transactions_loc / duration.count();

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);

    // Afficher les résultats du benchmark [Transactions totales avec le nombre de réussites et d'erreurs + en %, TPS transactions per second et temps moyen pour une transaction calculé]
    std::cout << "\n=== Résultat du benchmark pour " << nb_transactions << " transactions ===\n";
    std::cout << "Transactions réussies : " << successful_transactions_loc << " (" << (successful_transactions_loc * 100.0 / nb_transactions) << "%)\n";
    std::cout << "Transactions échouées : " << failed_transactions_loc << " (" << (failed_transactions_loc * 100.0 / nb_transactions) << "%)\n";
    std::cout << "Durée totale : " << duration.count() << " s\n";
    std::cout << "Transactions par seconde (TPS) : " << tps << "\n";
    std::cout << "Temps moyen par transaction : " << (duration.count() * 1000 / nb_transactions) << " ms\n\n";
}


void connect_to_server_max(int id) {
    // Créer un socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Erreur création socket");
    }
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    auto connect_start = std::chrono::high_resolution_clock::now();
    bool connected = connect_with_timeout(sock, &server_addr, CONNECTION_TIMEOUT_MS);
    auto connect_end = std::chrono::high_resolution_clock::now();
    
    if (!connected) {
        close(sock);
        throw std::runtime_error("Échec de la connexion (timeout)");
    }

    // Créer un objet SSL_CTX pour cette connexion
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        close(sock);
        throw std::runtime_error("Erreur création SSL_CTX");
    }

    // Créer un objet SSL en utilisant le contexte SSL_CTX
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        close(sock);
        throw std::runtime_error("Échec création SSL");
    }

    // Connecter le socket à SSL
    if (!SSL_set_fd(ssl, sock)) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        throw std::runtime_error("Erreur SSL_set_fd");
    }
    
    // Initier la connexion SSL
    int ssl_result = SSL_connect(ssl);
    if (ssl_result <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        throw std::runtime_error("Échec connexion SSL");
    }
    
    // Envoi d'une chaîne d'authentification fictive après une connexion SSL réussie, wallet aura donc le nom benchmark
    const char* auth_message = "ID:benchmark,TOKEN:abc123\n";
    int bytes_sent = SSL_write(ssl, auth_message, strlen(auth_message));
    if (bytes_sent <= 0) {
        int auth_err = SSL_get_error(ssl, bytes_sent);
        std::lock_guard<std::mutex> lock(cout_mutex);   //permet de pas mélanger tous les clients dans l'affichage 
        std::cerr << "Client #" << id << ": auth_failed, error = " << auth_err << "\n";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
    return;
    }
}

/* Benchmark 3 : utilise la fonction connect_to_server_max. On mesure le nombre de connexion client qu'il faut pour arriver à saturation du server. 
Le but ici est de générer pleins de clients à la suite sans jamais couper le socket ou la connexion SSL et voir quand le serveur ne peut plus supporter la charge. 
C'est une valeur importante dans un Serveur pour déterminer quelle capacité celui ci peut avoir et dépend de la machine sur lequel il tourne.*/
void test_max_connections(){
    std::vector<std::thread> threads;
    int i = 0;

    while (true) {
        try {
            threads.emplace_back(connect_to_server_max, i++);
            std::cout << i << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Thread creation failed at " << i << " connections: " << e.what() << std::endl;
            break;
        }
    }

    std::cout << "Total threads successfully launched: " << threads.size() << std::endl;

    // Ne pas joindre les threads pour les laisser vivre et saturer le serveur
    std::this_thread::sleep_for(std::chrono::seconds(10));
}

int main() {
    //------------------------CPS------------------------------
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



    //-----------------------------TPS--------------------------------
    LOG("Démarrage du benchmark pour 10 transactions", "INFO");
    test_transactions(10);

    LOG("Démarrage du benchmark pour 100 transactions", "INFO");
    test_transactions(100);

    LOG("Démarrage du benchmark pour 1 000 transactions", "INFO");
    test_transactions(1000);

    LOG("Démarrage du benchmark pour 10 000 transactions", "INFO");
    test_transactions(10000);

    LOG("Démarrage du benchmark pour 20 000 transactions", "INFO");
    test_transactions(20000);

    LOG("Démarrage du benchmark pour 1 000 000 transactions", "INFO");
    test_transactions(1000000);


    //--------------------------Surcharge-------------------------------
    //LOG("Démarrage du benchmark pour établir le nombre de connections maximum", "INFO");
    //test_max_connections();

    return 0;
}