#ifndef WEATHER_H
#define WEATHER_H

#include <string>
#include <cstring>
#include <algorithm>
#include <cctype>
#include "socket_compact.h"
#include "colors.h"

using namespace std;

#define API_HOST "nycu.waynewolf.tw"
#define API_PORT "80"
#define API_PATH "/weather/weather.do"

struct WeatherData {
    string time;
    string temperature;
    string humidity;
};

inline string format_weather(const string& location, const WeatherData* data) {
    return string(BOLD CYAN) + "--- Weather Report for " + location + " ---" + RESET + "\n" +
           BOLD "Time:        " RESET + data->time + "\n" +
           BOLD "Temperature: " RESET + data->temperature + " °C\n" +
           BOLD "Humidity:    " RESET + data->humidity + " %\n" +
           string(BOLD CYAN) + "---------------------------------" + RESET;
}

inline int get_weather(string location, WeatherData* data) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(API_HOST, API_PORT, &hints, &res) != 0) {
        LOG_ERROR("Could not resolve weather server address");
        return -1;
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!ISVALIDSOCKET(sock)) {
        freeaddrinfo(res);
        LOG_ERROR("Could not create socket for weather server");
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        CLOSESOCKET(sock);
        freeaddrinfo(res);
        LOG_ERROR("Could not connect to weather server");
        return -1;
    }
    freeaddrinfo(res);

    string body = "time=now&location=" + location;
    string request = 
        "POST " + string(API_PATH) + " HTTP/1.1\r\n"
        "Host: " + string(API_HOST) + "\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: " + to_string(body.length()) + "\r\n"
        "Connection: close\r\n\r\n" + body;

    int sent = 0;
    int total_to_send = (int)request.length();
    while (sent < total_to_send) {
        int n = send(sock, request.c_str() + sent, total_to_send - sent, 0);
        if (n <= 0) {
            CLOSESOCKET(sock);
            LOG_ERROR("Failed to send request to weather server");
            return -1;
        }
        sent += n;
    }

    string response;
    char buf[1024];
    while (true) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n < 0) {
            CLOSESOCKET(sock);
            LOG_ERROR("Failed to read from weather server");
            return -1;
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
        LOG_ERROR("Weather server returned non-200 status");
        return -1;
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

        data->time = time_part;
        data->temperature = temp_part;
        data->humidity = humid_part;
        return 0;
    }

    LOG_ERROR("Failed to parse weather data: %s", body_content.c_str());
    return -1;
}

#endif // WEATHER_H