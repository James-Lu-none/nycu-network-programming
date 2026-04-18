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
        string command, arg;
        if (space_pos == string::npos) {
            command = msg.substr(1);
            arg = "";
        } else {
            command = msg.substr(1, space_pos - 1);
            arg = msg.substr(space_pos + 1);
        }
        if (command == "help" || command == "h") {
            response = "Available commands: /help <or /h>, /nick <name> <or /n>, /list <or /l>, /quit <or /q>";
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
        } else if (command == "quit" || command == "q") {
            response = "Goodbye";
            client->send_packet(response.c_str(), response.length());
            client->set_inactive();
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
    for (auto it = all_clients.begin(); it != all_clients.end(); it++) {
        if ((*it) == client || !(*it)->is_active) continue;
        int result = (*it)->send_packet(full_message.c_str(), full_message.length());

        if (result < 0 && (*it)->type == TCP) {
            printf("TCP client %s send failed, set fd id: %d inactive\n", (*it)->username.c_str(), (*it)->sock);
            (*it)->set_inactive();
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

    while (true) {
        fd_set working_fds;
        FD_ZERO(&working_fds);
        FD_SET(tcp_listen, &working_fds);
        FD_SET(udp_socket, &working_fds);
        SOCKET max_fd = max(tcp_listen, udp_socket);
        
        time_t now = time(nullptr);

        for (auto it = all_clients.begin(); it != all_clients.end(); ) {
            // handle udp client timeout
            if ((*it)->type == UDP) {
                auto uc = static_pointer_cast<UDPClient>(*it);
                if (now - uc->last_seen > 60) (*it)->set_inactive(); 
            }
            // rebuild fd_set for next select and handle client removal (lazy removal)
            if ((*it)->is_active) {
                if ((*it)->type == TCP) {
                    FD_SET((*it)->sock, &working_fds);
                    max_fd = max(max_fd, (*it)->sock);
                }
                ++it;
            } else {
                // close socket if tcp client, we don't close udp socket since all udp client share the same socket
                if ((*it)->type == TCP) {
                    CLOSESOCKET((*it)->sock);
                }
                it = all_clients.erase(it);
            }
        }

        if (select(max_fd + 1, &working_fds, NULL, NULL, NULL) < 0) break;

        // Handle TCP connection
        if (FD_ISSET(tcp_listen, &working_fds)) {
            struct sockaddr_storage client_addr;
            socklen_t addr_len = sizeof(client_addr);
            SOCKET new_sock = accept(tcp_listen, (struct sockaddr*)&client_addr, &addr_len);
            if (ISVALIDSOCKET(new_sock)) {
                auto new_client = make_shared<TCPClient>(new_sock);
                all_clients.push_back(new_client);
            }
        }

        // Handle TCP client messages
        for (auto it = all_clients.begin(); it != all_clients.end(); it++) {
            shared_ptr<BaseClient> client = *it;
            // skip if not tcp client
            if (client->type != TCP) continue;
            SOCKET s = static_pointer_cast<TCPClient>(client)->sock;
            if (FD_ISSET(s, &working_fds)) {
                int bytes = recv(s, buffer, BUFFER_SIZE - 1, 0);
                if (bytes <= 0) {
                    client->set_inactive();
                } else {
                    buffer[bytes] = 0;
                    handleMessage(client, buffer, bytes);
                }
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