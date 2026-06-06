#ifndef SSL_HELPER_H
#define SSL_HELPER_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <strings.h>
#include "logging.h"
#include "colors.h"

using namespace std;

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

inline bool hostname_matches(const char* cn, const char* host) {
    if (strcasecmp(cn, host) == 0) return true;
    return false;
}

inline string get_asn1_time_string(const ASN1_TIME* tm) {
    if (!tm) return "N/A";
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return "N/A";
    ASN1_TIME_print(bio, tm);
    char buf[128];
    int len = BIO_read(bio, buf, sizeof(buf) - 1);
    if (len < 0) len = 0;
    buf[len] = '\0';
    BIO_free(bio);
    return string(buf);
}

inline string get_asn1_integer_string(const ASN1_INTEGER* serial) {
    if (!serial) return "N/A";
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return "N/A";
    i2a_ASN1_INTEGER(bio, serial);
    char buf[256];
    int len = BIO_read(bio, buf, sizeof(buf) - 1);
    if (len < 0) len = 0;
    buf[len] = '\0';
    BIO_free(bio);
    return string(buf);
}

inline int verify_ssl_cert (SSL* ssl, const char* host) {
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        LOG_ERROR("No server certificate presented.");
        return -1;
    }

    // check if expired or not yet valid
    const ASN1_TIME* not_before = X509_get0_notBefore(cert);
    const ASN1_TIME* not_after = X509_get0_notAfter(cert);
    if (X509_cmp_time(not_before, nullptr) > 0) {
        LOG_ERROR("Certificate is not yet valid.");
        X509_free(cert);
        return -1;
    }
    if (X509_cmp_time(not_after, nullptr) < 0) {
        LOG_ERROR("Certificate has expired.");
        X509_free(cert);
        return -1;
    }

    // check if the certificate's common name doesn't match the hostname
    X509_NAME* subject_name = X509_get_subject_name(cert);
    char cn[256] = {0};
    int cn_len = X509_NAME_get_text_by_NID(subject_name, NID_commonName, cn, sizeof(cn));
    if (cn_len < 0 || !hostname_matches(cn, host)) {
        LOG_ERROR("Certificate common name (%s) does not match hostname (%s).", cn_len >= 0 ? cn : "UNKNOWN", host);
        X509_free(cert);
        return -1;
    }

    // warn user if the certificate is self-signed (not issued by a trusted CA)
    X509_NAME* issuer_name = X509_get_issuer_name(cert);
    bool is_self_signed = (subject_name && issuer_name && X509_NAME_cmp(subject_name, issuer_name) == 0);
    if (is_self_signed) {
        printf(BOLD YELLOW "WARNING: The server certificate is self-signed (not issued by a trusted CA).\n" RESET);
    }

    X509_free(cert);
    return 0;
}

inline void print_ssl_cert_info(SSL* ssl) {
    if (!ssl) {
        printf("SSL connection not active.\n");
        return;
    }
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        printf("No server certificate presented.\n");
        return;
    }

    char subject_buf[256] = {0};
    char issuer_buf[256] = {0};
    X509_NAME_oneline(X509_get_subject_name(cert), subject_buf, sizeof(subject_buf) - 1);
    X509_NAME_oneline(X509_get_issuer_name(cert), issuer_buf, sizeof(issuer_buf) - 1);

    const ASN1_TIME* not_before = X509_get0_notBefore(cert);
    const ASN1_TIME* not_after = X509_get0_notAfter(cert);
    const ASN1_INTEGER* serial = X509_get_serialNumber(cert);

    X509_NAME* subj = X509_get_subject_name(cert);
    X509_NAME* iss = X509_get_issuer_name(cert);
    bool is_self_signed = (subj && iss && X509_NAME_cmp(subj, iss) == 0);

    printf("\n");
    printf(BOLD CYAN "--- TLS Server Certificate Information ---" RESET "\n");
    printf(BOLD "Subject: " RESET "%s\n", subject_buf);
    printf(BOLD "Issuer:  " RESET "%s\n", issuer_buf);
    printf(BOLD "Serial:  " RESET "%s\n", get_asn1_integer_string(serial).c_str());
    printf(BOLD "Validity:" RESET "\n");
    printf("  " BOLD "Not Before: " RESET "%s\n", get_asn1_time_string(not_before).c_str());
    printf("  " BOLD "Not After:  " RESET "%s\n", get_asn1_time_string(not_after).c_str());
    printf(BOLD "Status:   " RESET "%s\n", is_self_signed ? YELLOW "Self-Signed Certificate (Untrusted)" RESET : GREEN "Trusted / Verified Certificate" RESET);
    printf(BOLD CYAN "------------------------------------------" RESET);
    printf("\n\n");

    X509_free(cert);
}

#endif // SSL_HELPER_H
