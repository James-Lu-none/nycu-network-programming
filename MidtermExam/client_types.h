#ifndef CLIENT_TYPES_H
#define CLIENT_TYPES_H

#include "socket_compact.h"
#include <string>
#include <vector>
#include <memory>
#include <time.h>
#include <stdio.h>

using namespace std;

enum ClientType { TCP, UDP };

class BaseClient {
public:
    ClientType type;
    SOCKET sock;
    string username;
    time_t last_seen;
    BaseClient(ClientType t, SOCKET s) : type(t), sock(s) {
        last_seen = time(nullptr);
        username = "unknown";
    }
    virtual ~BaseClient() {}
    virtual int send_packet(const char* data, int len) = 0;
    virtual void set_username(const string& name) = 0;
};

class TCPClient : public BaseClient {
public:
    TCPClient(SOCKET s): BaseClient(TCP, s) {
        printf("TCP client connected, fd id: %d\n", s);
    }

    int send_packet(const char* data, int len) override {
        printf("Sending packet to TCP client %s, fd id: %d\n", username.c_str(), sock);
        return send(sock, data, len, 0);
    }

    void set_username(const string& name) override {
        username = name;
        printf("TCP client set username to %s, fd id: %d\n", name.c_str(), sock);
    }
};

class UDPClient : public BaseClient {
public:
    struct sockaddr_storage addr;
    socklen_t addr_len;

    UDPClient(SOCKET s, struct sockaddr_storage a, socklen_t l): BaseClient(UDP, s), addr(a), addr_len(l) {
        printf("UDP client connected, fd id: %d\n", s);
    }

    int send_packet(const char* data, int len) override {
        printf("Sending packet to UDP client %s, fd id: %d\n", username.c_str(), sock);
        return sendto(sock, data, len, 0, (struct sockaddr*)&addr, addr_len);
    }

    void set_username(const string& name) override {
        username = name;
        printf("UDP client set username to %s, fd id: %d\n", name.c_str(), sock);
    }
};

#endif // CLIENT_TYPES_H
