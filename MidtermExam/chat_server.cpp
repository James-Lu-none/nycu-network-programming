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
#include "colors.h"

// use shared_ptr so we dont have to worry about memory leak and Object Slicing
vector<shared_ptr<BaseClient>> all_clients;

void broadcastSystemMessage(const string& msg, shared_ptr<BaseClient> exclude = nullptr) {
    time_t now = time(nullptr);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", localtime(&now));
    
    string full_message = string(GRAY) + "[" + time_str + "] " + RESET + BOLD YELLOW "[SYSTEM] " + RESET + CYAN + msg + RESET;
    for (auto& client : all_clients) {
        if (client == exclude || !client->is_active) continue;
        client->send_packet(full_message.c_str(), full_message.length());
    }
}

void handleMessage(shared_ptr<BaseClient> client, const char* buffer, int len) {
    string msg(buffer, len);
    time_t now = time(nullptr);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", localtime(&now));
    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
    if (!msg.empty() && msg.back() == '\r') msg.pop_back();

    // if start with / , handle command
    if (msg[0] == '/') {
        string response;
        // split msg by space
        size_t space_pos = msg.find(' ');
        string command;
        vector<string> args;
        if (space_pos == string::npos) {
            command = msg.substr(1);
            args.clear();
        } else {
            command = msg.substr(1, space_pos - 1);
            // push arg until last space
            while (space_pos != string::npos) {
                size_t space_pos2 = msg.find(' ', space_pos + 1);
                if (space_pos2 != string::npos) {
                    args.push_back(msg.substr(space_pos + 1, space_pos2 - space_pos - 1));
                } else {
                    args.push_back(msg.substr(space_pos + 1));
                }
                space_pos = space_pos2;
            }
        }
        printf("Received command: %s from %s\n", command.c_str(), client->username.c_str());
        if (command == "help" || command == "h") {
            response = string(BOLD CYAN) + "Available commands: \n" + RESET +
            CYAN "/help <or /h>: show this message\n"
            "/nick <name> <or /n>: change nickname\n"
            "/list <or /l>: list all active users\n"
            "/dm <name> <message> <or /d>: send direct message to a user\n"
            "/ping <or /p>: update last seen time\n"
            "/quit <or /q>: quit the server" RESET;
        } else if (command == "list" || command == "l") {
            response = string(BOLD CYAN) + "Active users: " + RESET + WHITE;
            for (const auto& c : all_clients) {
                response += c->username + ", ";
            }
            response.pop_back();
            response.pop_back();
            response += RESET;
        } else if (command == "nick" || command == "n") {
            if (args.size() != 1) {
                response = string(RED) + "Usage: /nick <name>" + RESET;
                client->send_packet(response.c_str(), response.length());
                return;
            }
            client->set_username(args[0]);
            response = string(CYAN) + "Nickname changed to " + BOLD WHITE + args[0] + RESET;
        } else if (command == "quit" || command == "q") {
            response = string(CYAN) + "Goodbye" + RESET;
            client->set_inactive();
        } else if (command == "dm" || command == "d") {
            if (args.size() < 2) {
                response = string(RED) + "Usage: /dm <name> <message>" + RESET;
                client->send_packet(response.c_str(), response.length());
                return;
            }
            auto it = find_if(all_clients.begin(), all_clients.end(), [&](const shared_ptr<BaseClient>& c) {
                return c->username == args[0];
            });
            if (it == all_clients.end()) {
                response = string(RED) + "User not found" + RESET;
                client->send_packet(response.c_str(), response.length());
            } else {
                response = string(GRAY) + "[" + time_str + "] " + RESET + client->get_protocol_prefix() + 
                           " " + MAGENTA + "Direct message from " + BOLD + client->username + RESET + MAGENTA + ": ";
                for (size_t i = 1; i < args.size(); i++) {
                    response += args[i] + " ";
                }
                response += RESET;
                (*it)->send_packet(response.c_str(), response.length());
                
                // Also give feedback to the sender
                string feedback = string(CYAN) + "Message sent to " + BOLD WHITE + args[0] + RESET;
                client->send_packet(feedback.c_str(), feedback.length());
            }
            return;
        } else if (command == "ping" || command == "p") {
            response = "pong";
            client->last_seen = time(nullptr);
        } else {
            response = "Unknown command. Type /help for a list of commands.";
        }
        client->send_packet(response.c_str(), response.length());
        return;
    }

    // broadcast message
    string full_message = string(GRAY) + "[" + time_str + "] " + RESET + client->get_protocol_prefix() + 
                          " " + BOLD WHITE + client->username + RESET + " Said: " + msg;
    for (auto it = all_clients.begin(); it != all_clients.end(); it++) {
        if ((*it) == client || !(*it)->is_active) continue;
        int result = (*it)->send_packet(full_message.c_str(), full_message.length());

        if (result < 0 && (*it)->type == TCP) {
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

    printf("Server listening on port %s (TCP & UDP)\n", port);

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
        
        time_t now = time(nullptr);

        for (auto it = all_clients.begin(); it != all_clients.end(); ) {
            // handle udp client timeout
            if ((*it)->type == UDP) {
                auto uc = static_pointer_cast<UDPClient>(*it);
                if (now - uc->last_seen > 10) (*it)->set_inactive(); 
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
                broadcastSystemMessage((*it)->username + " left the chat");
                it = all_clients.erase(it);
            }
        }

        if (select(max_fd + 1, &working_fds, NULL, NULL, &tv) < 0) break;

        // Handle TCP connection
        if (FD_ISSET(tcp_listen, &working_fds)) {
            struct sockaddr_storage client_addr;
            socklen_t addr_len = sizeof(client_addr);
            SOCKET new_sock = accept(tcp_listen, (struct sockaddr*)&client_addr, &addr_len);
            if (ISVALIDSOCKET(new_sock)) {
                auto new_client = make_shared<TCPClient>(new_sock);
                all_clients.push_back(new_client);
                broadcastSystemMessage(new_client->username + " joined the chat via TCP", new_client);
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
                    broadcastSystemMessage(sender->username + " joined the chat via UDP", sender);
                }
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