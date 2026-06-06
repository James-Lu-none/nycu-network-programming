#ifndef WEATHER_H
#define WEATHER_H

#include <string>
#include <cstring>
#include <algorithm>
#include <cctype>
#include "socket_compact.h"
#include "colors.h"

using namespace std;

#define API_URL "http://nycu.waynewolf.tw/weather/weather.do"

inline string get_weather(string location) {
    // convert location to lowercase
    transform(location.begin(), location.end(), location.begin(), ::tolower);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo("nycu.waynewolf.tw", "80", &hints, &res) != 0) {
        return "Error: Could not resolve weather server address";
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(sock)) {
        freeaddrinfo(res);
        return "Error: Could not create socket for weather server";
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        CLOSESOCKET(sock);
        freeaddrinfo(res);
        return "Error: Could not connect to weather server";
    }
    freeaddrinfo(res);

    string body = "time=now&location=" + location;
    string request = 
        "POST /weather/weather.do HTTP/1.1\r\n"
        "Host: nycu.waynewolf.tw\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: " + to_string(body.length()) + "\r\n"
        "Connection: close\r\n\r\n" + body;

    int sent = 0;
    int total_to_send = (int)request.length();
    while (sent < total_to_send) {
        int n = send(sock, request.c_str() + sent, total_to_send - sent, 0);
        if (n <= 0) {
            CLOSESOCKET(sock);
            return "Error: Failed to send request to weather server";
        }
        sent += n;
    }

    string response;
    char buf[1024];
    while (true) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n < 0) {
            CLOSESOCKET(sock);
            return "Error: Failed to read from weather server";
        }
        if (n == 0) break;
        buf[n] = '\0';
        response.append(buf, n);
    }
    CLOSESOCKET(sock);

    size_t header_end = response.find("\r\n\r\n");
    string body_content;
    if (header_end != string::npos) {
        body_content = response.substr(header_end + 4);
    } else {
        header_end = response.find("\n\n");
        if (header_end != string::npos) {
            body_content = response.substr(header_end + 2);
        } else {
            body_content = response;
        }
    }

    if (response.find("HTTP/1.1 200") == string::npos && response.find("HTTP/1.0 200") == string::npos) {
        return "Error: Weather server returned non-200 status.";
    }

    // trim body_content
    while (!body_content.empty() && (body_content.back() == '\n' || body_content.back() == '\r' || body_content.back() == ' ')) {
        body_content.pop_back();
    }
    while (!body_content.empty() && (body_content.front() == '\n' || body_content.front() == '\r' || body_content.front() == ' ')) {
        body_content.erase(body_content.begin());
    }

    size_t comma1 = body_content.find(',');
    size_t comma2 = (comma1 != string::npos) ? body_content.find(',', comma1 + 1) : string::npos;

    if (comma1 != string::npos && comma2 != string::npos) {
        string time_part = body_content.substr(0, comma1);
        string temp_part = body_content.substr(comma1 + 1, comma2 - comma1 - 1);
        string humid_part = body_content.substr(comma2 + 1);

        string formatted = string(BOLD CYAN) + "--- Weather Report for " + location + " ---" + RESET + "\n" +
                                BOLD "Time:        " RESET + time_part + "\n" +
                                BOLD "Temperature: " RESET + temp_part + " °C\n" +
                                BOLD "Humidity:    " RESET + humid_part + " %\n" +
                                string(BOLD CYAN) + "---------------------------------" + RESET;
        return formatted;
    }

    return "Error: Failed to parse weather data: " + body_content;
}

#endif // WEATHER_H