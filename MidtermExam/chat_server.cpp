#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <stdio.h>
#include <cstring>
#include <string>

#include "socket_compact.h"
#include "client.h"

using namespace std;

#define MAX_CLIENTS 100
#define BUFFER_SIZE 4096

// use shared_ptr so we dont have to worry about memory leak and Object Slicing
vector<shared_ptr<BaseClient>> all_clients;
void handleMessage(shared_ptr<BaseClient> client, const char* buffer, int len) {
    string msg(buffer, len);
    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
    if (!msg.empty() && msg.back() == '\r') msg.pop_back();

    // if start with / , handle command
    if (msg[0] == '/') {
        string response;
        // split msg by space
        size_t space_pos = msg.find(' ');
        if (space_pos == string::npos) {
            space_pos = msg.length();
        }
        string command = msg.substr(1, space_pos - 1);
        string arg = msg.substr(space_pos + 1);
        if (command == "help" || command == "h") {
            response = "Available commands: /help <or /h>, /nick <name> <or /n>, /list <or /l>";
        } else if (command == "list" || command == "l") {
            response = "Online users: ";
            for (const auto& c : all_clients) {
                response += c->username + ", ";
            }
        } else if (command == "nick" || command == "n") {
            if (arg.empty()) {
                response = "Usage: /nick <name>";
                return;
            }
            client->set_username(arg);
            response = "Nickname changed to " + arg;
        } else {
            response = "Unknown command. Type /help for a list of commands.";
        }
        client->send_packet(response.c_str(), response.length());
        return;
    }

    // broadcast message
    time_t now = time(nullptr);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", localtime(&now));
    string full_message = string(time_str) + " " + client->username + " Said: " + msg;
    for (auto it = all_clients.begin(); it != all_clients.end(); ) {
        if ((*it) == client) {
            ++it;
            continue;
        }
        int result = (*it)->send_packet(full_message.c_str(), full_message.length());

        if (result < 0 && (*it)->type == TCP) {
            printf("TCP client %s disconnected, fd id: %d\n", (*it)->username.c_str(), (*it)->sock);
            CLOSESOCKET((*it)->sock);
            it = all_clients.erase(it);
        } else {
            ++it;
        }
    }
}

int main() {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
#endif

    const char* port = "8080";
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) return 1;

    // TCP Setup
    SOCKET tcp_listen = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(tcp_listen)) return 1;
    
    int opt = 1;
    setsockopt(tcp_listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(tcp_listen, res->ai_addr, res->ai_addrlen) < 0) return 1;
    if (listen(tcp_listen, 10) < 0) return 1;

    // UDP Setup
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *res_udp;
    if (getaddrinfo(NULL, port, &hints, &res_udp) != 0) return 1;
    SOCKET udp_socket = socket(res_udp->ai_family, res_udp->ai_socktype, res_udp->ai_protocol);
    if (!ISVALIDSOCKET(udp_socket)) return 1;
    if (bind(udp_socket, res_udp->ai_addr, res_udp->ai_addrlen) < 0) return 1;

    freeaddrinfo(res);
    freeaddrinfo(res_udp);

    cout << "Server listening on port " << port << " (TCP & UDP)" << endl;

    char buffer[BUFFER_SIZE];

    fd_set master_fds;
    FD_ZERO(&master_fds);

    // set read_fds bit for tcp_listen and udp_socket
    FD_SET(tcp_listen, &master_fds);
    FD_SET(udp_socket, &master_fds);
    
    SOCKET max_fd = max(tcp_listen, udp_socket);

    while (true) {
        fd_set working_fds = master_fds;
        if (select(max_fd + 1, &working_fds, NULL, NULL, NULL) < 0) break;

        // Handle TCP connection
        if (FD_ISSET(tcp_listen, &working_fds)) {
            struct sockaddr_storage client_addr;
            socklen_t addr_len = sizeof(client_addr);
            SOCKET new_sock = accept(tcp_listen, (struct sockaddr*)&client_addr, &addr_len);
            if (ISVALIDSOCKET(new_sock)) {
                auto new_client = make_shared<TCPClient>(new_sock);
                all_clients.push_back(new_client);
                FD_SET(new_sock, &master_fds);
                max_fd = max(max_fd, new_sock);
            }
        }

        // Handle TCP client messages
        for (auto it = all_clients.begin(); it != all_clients.end(); ) {
            shared_ptr<BaseClient> client = *it;
            bool disconnected = false;
            
            // skip if not tcp client
            if (client->type != TCP) {
                ++it;
                continue;
            }
            SOCKET s = static_pointer_cast<TCPClient>(client)->sock;
            if (FD_ISSET(s, &working_fds)) {
                int bytes = recv(s, buffer, BUFFER_SIZE - 1, 0);
                if (bytes <= 0) {
                    printf("TCP client %s disconnected, fd id: %d\n", client->username.c_str(), s);
                    CLOSESOCKET(s);
                    disconnected = true;
                } else {
                    buffer[bytes] = 0;
                    handleMessage(client, buffer, bytes);
                }
            }
            if (disconnected) {
                it = all_clients.erase(it);
                FD_CLR(s, &master_fds);
            } else {
                ++it;
            }
        }

        // Handle UDP connection and messages
        // (since UDP is connectionless, we only check if udp_socket fd is set and find or create UDP client)
        if (FD_ISSET(udp_socket, &working_fds)) {
            struct sockaddr_storage client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int bytes = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
            if (bytes > 0) {
                buffer[bytes] = 0;
                string msg(buffer);
                if (!msg.empty() && msg.back() == '\n') msg.pop_back();
                if (!msg.empty() && msg.back() == '\r') msg.pop_back();

                // find or create UDP client
                shared_ptr<UDPClient> sender = nullptr;
                for (const auto& c : all_clients) {
                    if (c->type == UDP) {
                        auto uc = static_pointer_cast<UDPClient>(c);
                        struct sockaddr_in* a = (struct sockaddr_in*)&client_addr;
                        struct sockaddr_in* b = (struct sockaddr_in*)&uc->addr;
                        // if ip and port are the same, then it is the same client
                        if (a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port) {
                            sender = uc;
                            break;
                        }
                    }
                }

                // if not found, create new UDP client
                if (!sender) {
                    sender = make_shared<UDPClient>(udp_socket, client_addr, addr_len);
                    char host[NI_MAXHOST];
                    char serv[NI_MAXSERV];
                    getnameinfo((struct sockaddr*)&client_addr, addr_len, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);
                    sender->set_username(string(host) + ":" + string(serv));
                    all_clients.push_back(sender);
                }

                // update last seen and handle message for new or found UDP client
                sender->last_seen = time(nullptr);
                handleMessage(sender, buffer, bytes);
            }
        }
    }

    CLOSESOCKET(tcp_listen);
    CLOSESOCKET(udp_socket);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}