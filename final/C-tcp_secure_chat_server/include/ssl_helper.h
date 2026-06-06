#ifndef SSL_HELPER_H
#define SSL_HELPER_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <stdio.h>
#include "logging.h"

inline bool generate_self_signed_cert(const char* key_path, const char* cert_path) {
    // generate a RSA private key
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!pctx) return false;
    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        LOG_ERROR("Failed to initialize RSA key generation");
        return false;
    }
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        LOG_ERROR("Failed to set RSA key generation bits");
        return false;
    }
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        LOG_ERROR("Failed to generate RSA key");
        return false;
    }
    EVP_PKEY_CTX_free(pctx);
    FILE *key_file = fopen(key_path, "wb");
    if (!key_file) {
        EVP_PKEY_free(pkey);
        LOG_ERROR("Failed to open private key file for writing");
        return false;
    }
    PEM_write_PrivateKey(key_file, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(key_file);
    LOG_INFO("Private key generated successfully");

    // create and configure the X509 certificate
    X509 *x509 = X509_new();
    if (!x509) {
        EVP_PKEY_free(pkey);
        LOG_ERROR("Failed to create X509 certificate");
        return false;
    }
    X509_set_version(x509, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);
    X509_set_pubkey(x509, pkey);
    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"localhost", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    // self-sign the certificate with private key
    if (X509_sign(x509, pkey, EVP_sha256()) == 0) {
        X509_free(x509);
        EVP_PKEY_free(pkey);
        LOG_ERROR("Failed to self-sign certificate");
        return false;
    }

    FILE *cert_file = fopen(cert_path, "wb");
    if (!cert_file) {
        X509_free(x509);
        EVP_PKEY_free(pkey);
        LOG_ERROR("Failed to open certificate file for writing");
        return false;
    }
    PEM_write_X509(cert_file, x509);
    fclose(cert_file);
    LOG_INFO("Certificate generated successfully");

    X509_free(x509);
    EVP_PKEY_free(pkey);
    return true;
}

inline bool ensure_certificates(const char* key_path, const char* cert_path) {
    FILE* kf = fopen(key_path, "r");
    FILE* cf = fopen(cert_path, "r");
    if (kf) fclose(kf);
    if (cf) fclose(cf);

    if (!kf || !cf) {
        LOG_INFO("SSL Certificate or Key not found. Generating self-signed certificate...");
        return generate_self_signed_cert(key_path, cert_path);
    }
    return true;
}

inline SSL_CTX* init_server_ssl_ctx(const char* cert_path, const char* key_path) {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        LOG_ERROR("Failed to create SSL context");
        return nullptr;
    }

    if (SSL_CTX_use_certificate_file(ssl_ctx, cert_path, SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR("Failed to load SSL certificate");
        SSL_CTX_free(ssl_ctx);
        return nullptr;
    }

    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR("Failed to load SSL private key");
        SSL_CTX_free(ssl_ctx);
        return nullptr;
    }

    if (!SSL_CTX_check_private_key(ssl_ctx)) {
        LOG_ERROR("Private key does not match public certificate");
        SSL_CTX_free(ssl_ctx);
        return nullptr;
    }

    return ssl_ctx;
}

inline SSL_CTX* init_client_ssl_ctx() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        LOG_ERROR("SSL_CTX creation failed");
        return nullptr;
    }
    return ssl_ctx;
}

#endif // SSL_HELPER_H
