#include "socket_compact.h"
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

#define BUFFER_SIZE 4096

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
#endif

    const char* host = argv[1];
    const char* port = argv[2];
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    bool use_tcp = true;
    SOCKET s = 0;

    printf("Attempting TCP connection to %s:%s...\n", host, port);
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        use_tcp = false;
    } else {
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (!ISVALIDSOCKET(s) || connect(s, res->ai_addr, res->ai_addrlen) < 0) {
            printf("TCP connection failed. Falling back to UDP...\n");
            use_tcp = false;
            if (ISVALIDSOCKET(s)) CLOSESOCKET(s);
        }
        freeaddrinfo(res);
    }

    struct sockaddr_storage udp_server_addr;
    socklen_t udp_addr_len = 0;

    if (!use_tcp) {
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(host, port, &hints, &res) != 0) {
            printf("UDP fallback failed: Could not resolve address\n");
            return 1;
        }
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (!ISVALIDSOCKET(s)) {
            printf("UDP fallback failed: Could not create socket\n");
            return 1;
        }
        memcpy(&udp_server_addr, res->ai_addr, res->ai_addrlen);
        udp_addr_len = res->ai_addrlen;
        freeaddrinfo(res);
        printf("Connected via UDP.\n");
    } else {
        printf("Connected via TCP.\n");
    }

    printf("You can now start chatting! Type 'exit' to quit.\n");

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
            printf("> ");
            fflush(stdout);
        }

        // Input from user (Linux/Unix)
#if !defined(_WIN32) && !defined(_WIN64)
        if (FD_ISSET(0, &read_fds)) {
            if (!fgets(buffer, BUFFER_SIZE, stdin)) break;
            string msg(buffer);
            if (msg.find("exit") == 0) break;
            
            if (use_tcp) {
                send(s, msg.c_str(), msg.length(), 0);
            } else {
                sendto(s, msg.c_str(), msg.length(), 0, (struct sockaddr*)&udp_server_addr, udp_addr_len);
            }
            printf("> ");
            fflush(stdout);
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