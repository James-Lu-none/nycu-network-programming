#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include "client.h"
#include "logging.h"
#include "colors.h"
#include "weather.h"

using namespace std;

class ChatServer {
public:
    vector<shared_ptr<BaseClient>> all_clients;

    void broadcastSystemMessage(const string& msg, shared_ptr<BaseClient> exclude = nullptr) {
        time_t now = time(nullptr);
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", localtime(&now));
        
        string full_message = string(GRAY) + "[" + time_str + "] " + RESET + BOLD YELLOW "[SYSTEM] " + RESET + CYAN + msg + RESET;
        for (auto& client : all_clients) {
            if (client == exclude || !client->is_active) continue;
            client->send_packet(full_message.c_str(), (int)full_message.length());
        }
    }

    void broadcastClientMessage(const string& msg, shared_ptr<BaseClient> sender) {
        time_t now = time(nullptr);
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", localtime(&now));
        
        string full_message = string(GRAY) + "[" + time_str + "] " + RESET + sender->get_protocol_prefix() + 
                              " " + BOLD WHITE + sender->username + RESET + " Said: " + msg;
        
        for (auto it = all_clients.begin(); it != all_clients.end(); it++) {
            if ((*it) == sender || !(*it)->is_active) continue;
            int result = (*it)->send_packet(full_message.c_str(), (int)full_message.length());
            if (result < 0) {
                LOG_ERROR("Failed to send packet to client %s", (*it)->username.c_str());
                (*it)->set_inactive();
            }
        }
        // Send ACK back to sender
        string ack = "[SENT]";
        sender->send_packet(ack.c_str(), (int)ack.length());
    }

    void handleCommand(const string& msg, shared_ptr<BaseClient> client) {
        time_t now = time(nullptr);
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", localtime(&now));
        string response;
        
        size_t space_pos = msg.find(' ');
        string command;
        vector<string> args;
        if (space_pos == string::npos) {
            command = msg.substr(1);
        } else {
            command = msg.substr(1, space_pos - 1);
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
        
        LOG_INFO("Received command: /%s from %s", command.c_str(), client->username.c_str());
        
        if (command == "help" || command == "h") {
            response = string(BOLD CYAN) + "Available commands: \n" + RESET +
            CYAN "/help <or /h>: show this message\n"
            "/nick <name> <or /n>: change nickname\n"
            "/list <or /l>: list all active users\n"
            "/dm <name> <message> <or /d>: send direct message to a user\n"
            "/ping <or /p>: update last seen time\n"
            "/tls: verify the server certificate\n"
            "/weather <location> <or /w>: obtain weather information\n"
            "/quit <or /q>: quit the server" RESET;
        } else if (command == "list" || command == "l") {
            response = string(BOLD CYAN) + "Active users: " + RESET + WHITE;
            for (const auto& c : all_clients) {
                if (c->is_active) response += c->username + ", ";
            }
            if (response.length() > 20) { // If any users were added
                response.pop_back();
                response.pop_back();
            }
            response += RESET;
        } else if (command == "nick" || command == "n") {
            if (args.size() != 1) {
                response = string(RED) + "Usage: /nick <name>" + RESET;
            } else {
                client->set_username(args[0]);
                response = string(CYAN) + "Nickname changed to " + BOLD WHITE + args[0] + RESET;
            }
        } else if (command == "quit" || command == "q") {
            response = string(CYAN) + "Goodbye" + RESET;
            client->set_inactive();
        } else if (command == "dm" || command == "d") {
            if (args.size() < 2) {
                response = string(RED) + "Usage: /dm <name> <message>" + RESET;
            } else {
                auto it = find_if(all_clients.begin(), all_clients.end(), [&](const shared_ptr<BaseClient>& c) {
                    return c->username == args[0] && c->is_active;
                });
                if (it == all_clients.end()) {
                    response = string(RED) + "User not found" + RESET;
                } else {
                    string dm_payload = string(GRAY) + "[" + time_str + "] " + RESET + client->get_protocol_prefix() + 
                                       " " + MAGENTA + "Direct message from " + BOLD + client->username + RESET + MAGENTA + ": ";
                    for (size_t i = 1; i < args.size(); i++) {
                        dm_payload += args[i] + " ";
                    }
                    dm_payload += RESET;
                    (*it)->send_packet(dm_payload.c_str(), (int)dm_payload.length());
                    response = string(CYAN) + "Message sent to " + BOLD WHITE + args[0] + RESET;
                }
            }
        } else if (command == "ping" || command == "p") {
            response = "[PONG]";
            client->last_seen = time(nullptr);
        } else if (command == "tls") {
            response = string(CYAN) + "Verifying TLS certificate on client side." + RESET;
        } else if (command == "weather" || command == "w") {
            if (args.empty()) {
                response = string(RED) + "Usage: /weather <location> (taipei, taoyuan, hsinchu, miaoli)" + RESET;
            } else {
                string loc = args[0];
                for (char &c : loc) c = tolower(c);
                if (loc != "taipei" && loc != "taoyuan" && loc != "hsinchu" && loc != "miaoli") {
                    response = string(RED) + "Invalid location. Choose from taipei, taoyuan, hsinchu, miaoli." + RESET;
                } else {
                    response = get_weather(loc);
                }
            }
        } else {
            response = string(RED) + "Unknown command. Type /help for a list of commands." + RESET;
        }
        
        if (!response.empty()) {
            client->send_packet(response.c_str(), (int)response.length());
        }
    }

    void handleMessage(shared_ptr<BaseClient> client, const char* buffer, int len) {
        string msg(buffer, len);
        if (!msg.empty() && msg.back() == '\n') msg.pop_back();
        if (!msg.empty() && msg.back() == '\r') msg.pop_back();

        if (!msg.empty() && msg[0] == '/') {
            handleCommand(msg, client);
        } else if (!msg.empty()) {
            broadcastClientMessage(msg, client);
        }
    }

    void cleanup() {
        time_t now = time(nullptr);
        for (auto it = all_clients.begin(); it != all_clients.end(); ) {
            if ((*it)->type == UDP) {
                auto uc = static_pointer_cast<UDPClient>(*it);
                if (now - uc->last_seen > 10) (*it)->set_inactive(); 
            }
            
            if (!(*it)->is_active) {
                if ((*it)->type == TCP) {
                    CLOSESOCKET((*it)->sock);
                }
                broadcastSystemMessage((*it)->username + " left the chat");
                LOG_INFO("client %s was lazy removed from all_clients", (*it)->username.c_str());
                it = all_clients.erase(it);
            } else {
                ++it;
            }
        }
    }
};

#endif // SERVER_H
