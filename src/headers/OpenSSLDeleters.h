#ifndef OPENSSL_DELETERS_H
#define OPENSSL_DELETERS_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <memory>

// Custom deleter pour SSL
struct SSLDeleter {
    void operator()(SSL* ssl) const {
        if (ssl) {
             SSL_free(ssl);
        }
    }
};

// Custom deleter pour SSL_CTX
struct SSLCTXDeleter {
    void operator()(SSL_CTX* ctx) const {
        if (ctx) {
             SSL_CTX_free(ctx);
        }
    }
};

// Types de pointeurs uniques utilisant les custom deleters
using UniqueSSL = std::unique_ptr<SSL, SSLDeleter>;
using UniqueSSLCTX = std::unique_ptr<SSL_CTX, SSLCTXDeleter>;

#endif