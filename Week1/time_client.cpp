#if defined(_WIN32) || defined(_WIN64)
    // define all network programming related libraries and configurations for Windows
    #ifdef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00
    #endif

    #include <winsock2.h>
    #include <ws2tcpip.h>
    // Link with the Ws2_32.lib library
    #pragma comment(lib, "ws2_32.lib")
#else
    // define all network programming related libraries and configurations for Unix/Linux
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
#endif

// define some common macros for cross-platform compatibility
#if defined(_WIN32) || defined(_WIN64)
    #define CLOSESOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#else
    #define CLOSESOCKET close
    #define SOCKET_ERROR_CODE errno
    #define ISVALIDSOCKET(s) ((s) >= 0)
    #define SOCKET int
#endif

#include <iostream>
#include <cstring>
#include <string>
#include <time.h>
#include <stdio.h>

using namespace std;

// A simple TCP client that connects to the time server on localhost:8080and receives the current time
int main() {
// Initialize Winsock (for Windows)
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    // Initialize Winsock version 2.2
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << endl;
        return 1;
    }
#else
    // No initialization needed for Unix/Linux
#endif
        
    while (true) {
        // printf("setting up local address...\n");
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // IPv4
        hints.ai_socktype = SOCK_STREAM; // TCP
        hints.ai_flags = AI_PASSIVE; // For wildcard IP address
        struct addrinfo* bind_address;
        int getaddrinfo_result = getaddrinfo(
            "localhost", // hostname
            "8080", // service (port)
            &hints, // hints
            &bind_address // result
        );
        if (getaddrinfo_result != 0) {
            fprintf(stderr, "getaddrinfo failed: %d\n", getaddrinfo_result);
            return 1;
        }
        // printf("creating socket...\n");
        SOCKET socket_client;
        socket_client = socket(
            bind_address->ai_family,
            bind_address->ai_socktype,
            bind_address->ai_protocol
        );
        if (!ISVALIDSOCKET(socket_client)) {
            fprintf(stderr, "socket client failed: %d\n", SOCKET_ERROR_CODE);   
            return 1;
        }
        // printf("connecting...\n");
        int connect_result = connect(
            socket_client,
            bind_address->ai_addr,
            bind_address->ai_addrlen
        );
        if (connect_result < SOCKET_ERROR_CODE) {
            fprintf(stderr, "connect failed: %d\n", SOCKET_ERROR_CODE);
            return 1;
        }
        // We don't need the bind_address anymore
        freeaddrinfo(bind_address);
        int byte_recv = 0;
        char recv_buffer[1024];
        byte_recv = recv(
            socket_client,
            recv_buffer,
            sizeof(recv_buffer) - 1,
            0
        );
        if (byte_recv < 0) {
            fprintf(stderr, "recv failed: %d\n", SOCKET_ERROR_CODE);
            return 1;
        }
        recv_buffer[byte_recv] = '\0'; // null-terminate the received string
        printf("\033[2J\033[1;1H");
        printf("Received time from server: %s\n", recv_buffer);
        CLOSESOCKET(socket_client);
        // clear screen for the next connection
        
    }
    
    #if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
    #else
        // No cleanup needed for Unix/Linux
    #endif
}