#include "socket_compact.h"
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

#define BUFFER_SIZE 4096

int try_tcp_connection(const char* host, const char* port, SOCKET* s_out) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        printf("TCP failed: Could not resolve address\n");
        return 1;
    }
    *s_out = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(*s_out)) {
        printf("TCP failed: Could not create socket\n");
        return 1;
    }
    if (connect(*s_out, res->ai_addr, res->ai_addrlen) < 0) {
        printf("TCP connection failed. Falling back to UDP...\n");
        if (ISVALIDSOCKET(*s_out)) CLOSESOCKET(*s_out);
        return 1;
    }
    freeaddrinfo(res);
    printf("Connected via TCP.\n");
    return 0;
}

int try_udp_connection(const char* host, const char* port, SOCKET* s_out, struct sockaddr_storage* addr, socklen_t* len) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        printf("UDP failed: Could not resolve address\n");
        return 1;
    }
    *s_out = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(*s_out)) {
        printf("UDP failed: Could not create socket\n");
        return 1;
    }
    memcpy(addr, res->ai_addr, res->ai_addrlen);
    *len = res->ai_addrlen;
    freeaddrinfo(res);
    printf("Connected via UDP.\n");
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <host> <username> [port] [protocol]\n", argv[0]);
        return 1;
    }
    const char* host = argv[1];
    const char* username = argv[2];
    const char* port = "8080";
    const char* protocol = "tcp";
    if (argc >= 4) {
        port = argv[3];
    }
    if (argc >= 5) {
        protocol = argv[4];
    }
    printf("Host: %s, User: %s, Port: %s, Proto: %s\n", host, username, port, protocol);

#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
#endif

    SOCKET s = 0;
    struct sockaddr_storage server_addr;
    socklen_t addr_len = 0;
    bool use_tcp = false;
    switch (protocol[0]) {
        case 't':
            if (try_tcp_connection(host, port, &s) != 0) {
                printf("TCP connection failed. Falling back to UDP...\n");
                if (try_udp_connection(host, port, &s, &server_addr, &addr_len) != 0) {
                    return 1;
                }
            }
            use_tcp = true;
            break;
        case 'u':
            if (try_udp_connection(host, port, &s, &server_addr, &addr_len) != 0) {
                return 1;
            }
            use_tcp = false;
            break;
        default:
            printf("Invalid protocol. Use 'tcp' or 'udp'.\n");
            return 1;
    }

    // set username with command /nick
    string msg = "/nick " + string(username);
    if (use_tcp) {
        send(s, msg.c_str(), msg.length(), 0);
    } else {
        sendto(s, msg.c_str(), msg.length(), 0, (struct sockaddr*)&server_addr, addr_len);
    }

    printf("You can now start chatting! Type /help for commands.\n");

    char buffer[BUFFER_SIZE];
    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(s, &read_fds);
#if !defined(_WIN32) && !defined(_WIN64)
        FD_SET(0, &read_fds); // STDIN
#endif

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms

        int select_res = select(s + 1, &read_fds, NULL, NULL, &tv);
        if (select_res < 0) break;

        // Message from server
        if (FD_ISSET(s, &read_fds)) {
            int bytes;
            if (use_tcp) {
                bytes = recv(s, buffer, BUFFER_SIZE - 1, 0);
            } else {
                bytes = recvfrom(s, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
            }

            if (bytes <= 0) {
                if (use_tcp) printf("Server disconnected.\n");
                break;
            }
            buffer[bytes] = 0;
            printf("%s\n", buffer);
            fflush(stdout);
        }

        // Input from user (Linux/Unix)
#if !defined(_WIN32) && !defined(_WIN64)
        if (FD_ISSET(0, &read_fds)) {
            printf("> ");
            fflush(stdout);
            if (!fgets(buffer, BUFFER_SIZE, stdin)) break;
            string msg(buffer);
            if (msg.find("/quit") == 0) break;
            
            if (use_tcp) {
                send(s, msg.c_str(), msg.length(), 0);
            } else {
                sendto(s, msg.c_str(), msg.length(), 0, (struct sockaddr*)&server_addr, addr_len);
            }
        }
#else
#endif
    }

    CLOSESOCKET(s);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}