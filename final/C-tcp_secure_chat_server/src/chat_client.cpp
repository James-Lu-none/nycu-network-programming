#include "socket_compact.h"
#include "colors.h"
#include "logging.h"
#include "ssl_helper.h"
#include <iostream>
#include <string>
#include <cstring>
#include <strings.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <signal.h>
#endif

using namespace std;

#define BUFFER_SIZE 4096

int try_tcp_connection(const char* host, const char* port, SOCKET* s_out) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        LOG_ERROR("TCP failed: Could not resolve address");
        return 1;
    }
    *s_out = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(*s_out)) {
        LOG_ERROR("TCP failed: Could not create socket");
        return 1;
    }
    if (connect(*s_out, res->ai_addr, res->ai_addrlen) < 0) {
        LOG_ERROR("TCP connection failed.");
        if (ISVALIDSOCKET(*s_out)) CLOSESOCKET(*s_out);
        return 1;
    }
    freeaddrinfo(res);
    LOG_INFO("Connected via TCP.");
    return 0;
}

int try_udp_connection(const char* host, const char* port, SOCKET* s_out, struct sockaddr_storage* addr, socklen_t* len) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        LOG_ERROR("UDP failed: Could not resolve address");
        return 1;
    }
    *s_out = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(*s_out)) {
        LOG_ERROR("UDP failed: Could not create socket");
        freeaddrinfo(res);
        return 1;
    }
    // use command /ping to do 2-way handshake
    sendto(*s_out, "/ping", 5, 0, res->ai_addr, res->ai_addrlen);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(*s_out, &read_fds);
    struct timeval tv;
    tv.tv_sec = 3; // 3 seconds timeout
    tv.tv_usec = 0;

    if (select((int)*s_out + 1, &read_fds, NULL, NULL, &tv) > 0) {
        char buffer[100];
        int bytes = recvfrom(*s_out, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
        if (bytes > 0) {
            buffer[bytes] = 0;
            if (strncmp(buffer, "[PONG]", 6) == 0) {
                memcpy(addr, res->ai_addr, res->ai_addrlen);
                *len = res->ai_addrlen;
                freeaddrinfo(res);
                LOG_INFO("Connected via UDP.");
                return 0;
            }
        }
    }

    LOG_ERROR("UDP connection handshake failed");
    freeaddrinfo(res);
    CLOSESOCKET(*s_out);
    return 1;
}

int main(int argc, char* argv[]) {
#if !defined(_WIN32) && !defined(_WIN64)
    signal(SIGPIPE, SIG_IGN);
#endif
    if (argc < 3) {
        LOG_ERROR("Usage: %s <host> <username> [port] [protocol]", argv[0]);
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
    LOG_INFO("Host: %s, User: %s, Port: %s, Proto: %s", host, username, port, protocol);

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
                LOG_INFO("Falling back to UDP...");
                if (try_udp_connection(host, port, &s, &server_addr, &addr_len) != 0) return 1;
            } else {
                use_tcp = true;
            }
            break;
        case 'u':
            if (try_udp_connection(host, port, &s, &server_addr, &addr_len) != 0) return 1;
            use_tcp = false;
            break;
        default:
            LOG_ERROR("Invalid protocol. Use 'tcp' or 'udp'.");
            return 1;
    }

    // Initialize SSL if using TCP
    SSL* ssl = nullptr;
    SSL_CTX* ssl_ctx = nullptr;
    if (use_tcp) {
        ssl_ctx = init_client_ssl_ctx();
        if (!ssl_ctx) {
            CLOSESOCKET(s);
            return 1;
        }

        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            LOG_ERROR("SSL object creation failed");
            SSL_CTX_free(ssl_ctx);
            CLOSESOCKET(s);
            return 1;
        }

        SSL_set_fd(ssl, s);
        if (SSL_connect(ssl) <= 0) {
            LOG_ERROR("SSL/TLS handshake failed");
            SSL_free(ssl);
            SSL_CTX_free(ssl_ctx);
            CLOSESOCKET(s);
            return 1;
        }

        if (verify_ssl_cert(ssl, host) != 0) {
            SSL_free(ssl);
            SSL_CTX_free(ssl_ctx);
            CLOSESOCKET(s);
            return 1;
        }
        LOG_INFO("SSL/TLS connection established successfully.");
    }

    // set username with command /nick
    string msg = "/nick " + string(username);
    if (use_tcp) {
        if (ssl) {
            SSL_write(ssl, msg.c_str(), msg.length());
        } else {
            send(s, msg.c_str(), msg.length(), 0);
        }
    } else {
        sendto(s, msg.c_str(), msg.length(), 0, (struct sockaddr*)&server_addr, addr_len);
    }

    LOG_INFO("You can now start chatting! Type /help for commands.");

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms

    time_t last_ping = time(nullptr);
    int ping_fail_count = 0;
    string last_sent_msg = "";

    char buffer[BUFFER_SIZE];

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(s, &read_fds);

        int max_fd = (int)s;

#if !defined(_WIN32) && !defined(_WIN64)
        FD_SET(0, &read_fds);
        max_fd = max((int)s, 0);
#endif

        int select_res = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (select_res > 0 && FD_ISSET(s, &read_fds)) {
            int bytes = 0;
            if (use_tcp) {
                if (ssl) {
                    bytes = SSL_read(ssl, buffer, BUFFER_SIZE - 1);
                } else {
                    bytes = recv(s, buffer, BUFFER_SIZE - 1, 0);
                }
            } else {
                bytes = recvfrom(s, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
            }

            if (bytes <= 0) {
                LOG_ERROR("Disconnected from server.");
                break;
            }
            buffer[bytes] = 0;

            if (strncmp(buffer, "[PONG]", 6) == 0) {
                ping_fail_count = 0;
            } else if (strncmp(buffer, "[SENT]", 6) == 0) {
                if (!last_sent_msg.empty()) {
                    printf("\033[s\033[1A\r" BOLD GREEN "> " RESET "%s " GRAY "(message sent)      " RESET "\033[u", last_sent_msg.c_str());
                    fflush(stdout);
                }
            } else {
                printf("\r%s\n" BOLD GREEN "> " RESET, buffer);
                fflush(stdout);
            }
        }

        // send /ping to server every 5 seconds
        time_t now = time(nullptr);
        if (now - last_ping >= 5) {
            if (use_tcp) {
                if (ssl) {
                    SSL_write(ssl, "/ping", 5);
                } else {
                    send(s, "/ping", 5, 0);
                }
            } else {
                sendto(s, "/ping", 5, 0, (struct sockaddr*)&server_addr, addr_len);
            }
            last_ping = now;
            ping_fail_count++;
        }

        if (ping_fail_count >= 3) {
            LOG_ERROR("Server timeout after 3 ping failures.");
            break;
        }

        bool has_input = false;
#if !defined(_WIN32) && !defined(_WIN64)
        if (select_res > 0 && FD_ISSET(0, &read_fds)) has_input = true;
#else
        if (_kbhit()) has_input = true;
#endif

        if (has_input) {
            if (!fgets(buffer, BUFFER_SIZE, stdin)) break;
            string msg(buffer);
            msg.erase(msg.find_last_not_of("\n\r") + 1);

            if (msg.empty()) {
                printf(BOLD GREEN "> " RESET); fflush(stdout);
                continue;
            }

            if (msg == "/tls") {
                if (use_tcp && ssl) {
                    print_ssl_cert_info(ssl);
                } else {
                    printf(RED "TLS is not active on this connection.\n" RESET);
                }
                printf(BOLD GREEN "> " RESET); fflush(stdout);
                continue;
            }

            last_sent_msg = msg;
            if (use_tcp) {
                if (ssl) {
                    SSL_write(ssl, msg.c_str(), (int)msg.length());
                } else {
                    send(s, msg.c_str(), (int)msg.length(), 0);
                }
            } else {
                sendto(s, msg.c_str(), (int)msg.length(), 0, (struct sockaddr*)&server_addr, addr_len);
            }
            
            if (msg == "/quit" || msg == "/q") break;

            if (msg[0] != '/'){
                printf("\033[1A\r" BOLD GREEN "> " RESET "%s " GRAY "(sending message...)" RESET "\n" BOLD GREEN "> " RESET, msg.c_str());
            } else {
                printf(BOLD GREEN "> " RESET);
            }
            fflush(stdout);
        }
    }

    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
    }

    CLOSESOCKET(s);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}