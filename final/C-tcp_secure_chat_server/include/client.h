#ifndef CLIENT_TYPES_H
#define CLIENT_TYPES_H

#include "socket_compact.h"
#include "colors.h"
#include "logging.h"
#include <string>
#include <vector>
#include <memory>
#include <time.h>
#include <stdio.h>

using namespace std;

enum ClientType { TCP, UDP };

void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *(dest) = '\0';
}

class BaseClient {
public:
    ClientType type;
    SOCKET sock;
    string username;
    time_t last_seen;
    bool is_active;
    BaseClient(ClientType t, SOCKET s) : type(t), sock(s) {
        last_seen = time(nullptr);
        char rand_str_buf[10];
        rand_str(rand_str_buf, sizeof(rand_str_buf)-1);
        username = "Guest-" + string(rand_str_buf);
        is_active = true;
    }
    virtual ~BaseClient() {}
    virtual int send_packet(const char* data, int len) = 0;
    virtual void set_username(const string& name) = 0;
    virtual void set_inactive() = 0;
    virtual string get_protocol_prefix() const = 0;
};

#include <openssl/ssl.h>

class TCPClient : public BaseClient {
public:
    SSL *ssl;

    TCPClient(SOCKET s, SSL *ssl_ptr = nullptr): BaseClient(TCP, s), ssl(ssl_ptr) {
        LOG_INFO("TCP client connected, fd id: %d, username: %s", (int)s, username.c_str());
    }

    ~TCPClient() override {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
    }

    int send_packet(const char* data, int len) override {
        LOG_DEBUG("Sending packet to TCP client %s, fd id: %d", username.c_str(), (int)sock);
        if (ssl) {
            return SSL_write(ssl, data, len);
        }
        return send(sock, data, len, 0);
    }

    void set_username(const string& name) override {
        username = name;
        LOG_INFO("TCP client set username to %s, fd id: %d", name.c_str(), (int)sock);
    }

    void set_inactive() override {
        LOG_INFO("TCP client %s, fd id: %d is inactive", username.c_str(), (int)sock);
        is_active = false;
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }
    }

    string get_protocol_prefix() const override {
        return string(BOLD GREEN "[TCP]" RESET);
    }
};

class UDPClient : public BaseClient {
public:
    struct sockaddr_storage addr;
    socklen_t addr_len;

    UDPClient(SOCKET s, struct sockaddr_storage a, socklen_t l): BaseClient(UDP, s), addr(a), addr_len(l) {
        LOG_INFO("UDP client connected, fd id: %d, username: %s", (int)s, username.c_str());
    }

    int send_packet(const char* data, int len) override {
        LOG_DEBUG("Sending packet to UDP client %s, fd id: %d", username.c_str(), (int)sock);
        return sendto(sock, data, len, 0, (struct sockaddr*)&addr, addr_len);
    }

    void set_username(const string& name) override {
        username = name;
        LOG_INFO("UDP client set username to %s, fd id: %d", name.c_str(), (int)sock);
    }

    void set_inactive() override {
        LOG_INFO("UDP client %s, fd id: %d is inactive", username.c_str(), (int)sock);
        is_active = false;
    }

    string get_protocol_prefix() const override {
        return string(BOLD BLUE "[UDP]" RESET);
    }
};

#endif // CLIENT_TYPES_H
