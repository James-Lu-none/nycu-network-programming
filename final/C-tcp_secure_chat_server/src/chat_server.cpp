#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <stdio.h>
#include <cstring>
#include <string>

#if !defined(_WIN32) && !defined(_WIN64)
#include <signal.h>
#endif

#include "socket_compact.h"
#include "client.h"
#include "colors.h"
#include "server.h"
#include "ssl_helper.h"

using namespace std;

#define MAX_CLIENTS 100
#define BUFFER_SIZE 4096

int main() {
#if !defined(_WIN32) && !defined(_WIN64)
    signal(SIGPIPE, SIG_IGN);
#endif
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("WSAStartup failed");
        return 1;
    }
#endif

    const char* key_path = "server.key";
    const char* cert_path = "server.crt";

    // Ensure self-signed certificate and key exist
    if (!ensure_certificates(key_path, cert_path)) {
        LOG_ERROR("Failed to generate or locate self-signed certificate");
        return 1;
    }

    // Initialize SSL context
    SSL_CTX* ssl_ctx = init_server_ssl_ctx(cert_path, key_path);
    if (!ssl_ctx) {
        LOG_ERROR("Failed to initialize server SSL context");
        return 1;
    }

    const char* port = "8080";
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        LOG_ERROR("getaddrinfo failed for TCP");
        SSL_CTX_free(ssl_ctx);
        return 1;
    }

    // TCP Setup
    SOCKET tcp_listen = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(tcp_listen)) {
        LOG_ERROR("TCP socket creation failed");
        freeaddrinfo(res);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    int opt = 1;
    setsockopt(tcp_listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(tcp_listen, res->ai_addr, res->ai_addrlen) < 0) {
        LOG_ERROR("TCP bind failed");
        freeaddrinfo(res);
        CLOSESOCKET(tcp_listen);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    if (listen(tcp_listen, 10) < 0) {
        LOG_ERROR("TCP listen failed");
        freeaddrinfo(res);
        CLOSESOCKET(tcp_listen);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }

    // UDP Setup
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *res_udp;
    if (getaddrinfo(NULL, port, &hints, &res_udp) != 0) {
        LOG_ERROR("getaddrinfo failed for UDP");
        freeaddrinfo(res);
        CLOSESOCKET(tcp_listen);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    SOCKET udp_socket = socket(res_udp->ai_family, res_udp->ai_socktype, res_udp->ai_protocol);
    if (!ISVALIDSOCKET(udp_socket)) {
        LOG_ERROR("UDP socket creation failed");
        freeaddrinfo(res);
        freeaddrinfo(res_udp);
        CLOSESOCKET(tcp_listen);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    if (bind(udp_socket, res_udp->ai_addr, res_udp->ai_addrlen) < 0) {
        LOG_ERROR("UDP bind failed");
        freeaddrinfo(res);
        freeaddrinfo(res_udp);
        CLOSESOCKET(tcp_listen);
        CLOSESOCKET(udp_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }

    freeaddrinfo(res);
    freeaddrinfo(res_udp);

    LOG_INFO("Server listening on port %s (TCP with SSL/TLS & UDP)", port);

    ChatServer chat_server;
    char buffer[BUFFER_SIZE];

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms

    while (true) {
        fd_set working_fds;
        FD_ZERO(&working_fds);
        FD_SET(tcp_listen, &working_fds);
        FD_SET(udp_socket, &working_fds);
        SOCKET max_fd = max(tcp_listen, udp_socket);
        
        // Handle client timeouts and lazy removal
        chat_server.cleanup();

        // Build fd_set for select
        for (const auto& client : chat_server.all_clients) {
            if (client->is_active && client->type == TCP) {
                FD_SET(client->sock, &working_fds);
                max_fd = max(max_fd, client->sock);
            }
        }

        if (select((int)max_fd + 1, &working_fds, NULL, NULL, &tv) < 0) break;

        // Handle TCP connection
        if (FD_ISSET(tcp_listen, &working_fds)) {
            struct sockaddr_storage client_addr;
            socklen_t addr_len = sizeof(client_addr);
            SOCKET new_sock = accept(tcp_listen, (struct sockaddr*)&client_addr, &addr_len);
            if (ISVALIDSOCKET(new_sock)) {
                SSL* ssl = SSL_new(ssl_ctx);
                SSL_set_fd(ssl, new_sock);
                if (SSL_accept(ssl) <= 0) {
                    LOG_ERROR("SSL handshake failed for new connection");
                    ERR_print_errors_fp(stderr);
                    SSL_free(ssl);
                    CLOSESOCKET(new_sock);
                } else {
                    auto new_client = make_shared<TCPClient>(new_sock, ssl);
                    chat_server.all_clients.push_back(new_client);
                    chat_server.broadcastSystemMessage(new_client->username + " joined the chat via TCP (SSL/TLS)", new_client);
                }
            }
        }

        // Handle TCP client messages
        for (auto& client : chat_server.all_clients) {
            if (client->type == TCP && client->is_active) {
                if (FD_ISSET(client->sock, &working_fds)) {
                    auto tc = static_pointer_cast<TCPClient>(client);
                    int bytes = 0;
                    if (tc->ssl) {
                        bytes = SSL_read(tc->ssl, buffer, BUFFER_SIZE - 1);
                    } else {
                        bytes = recv(client->sock, buffer, BUFFER_SIZE - 1, 0);
                    }
                    
                    if (bytes <= 0) {
                        client->set_inactive();
                    } else {
                        buffer[bytes] = 0;
                        chat_server.handleMessage(client, buffer, bytes);
                    }
                }
            }
        }

        // Handle UDP messages
        if (FD_ISSET(udp_socket, &working_fds)) {
            struct sockaddr_storage client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int bytes = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
            if (bytes > 0) {
                buffer[bytes] = 0;

                // find or create UDP client
                shared_ptr<UDPClient> sender = nullptr;
                for (const auto& c : chat_server.all_clients) {
                    if (c->type == UDP) {
                        auto uc = static_pointer_cast<UDPClient>(c);
                        struct sockaddr_in* a = (struct sockaddr_in*)&client_addr;
                        struct sockaddr_in* b = (struct sockaddr_in*)&uc->addr;
                        if (a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port) {
                            sender = uc;
                            break;
                        }
                    }
                }

                if (!sender) {
                    sender = make_shared<UDPClient>(udp_socket, client_addr, addr_len);
                    chat_server.all_clients.push_back(sender);
                    chat_server.broadcastSystemMessage(sender->username + " joined the chat via UDP", sender);
                }
                chat_server.handleMessage(sender, buffer, bytes);
            }
        }
    }

    CLOSESOCKET(tcp_listen);
    CLOSESOCKET(udp_socket);
    SSL_CTX_free(ssl_ctx);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}