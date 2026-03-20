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

    printf("setting up local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; // For wildcard IP address
    hints.ai_protocol = IPPROTO_TCP; // TCP protocol

    struct addrinfo* bind_address;
    int getaddrinfo_result = getaddrinfo(
        0, // use the local IP address
        "8080", // port number to listen on
        &hints, // hints that tells the type of socket we want
        &bind_address // the bind_address struct to hold the server address
    );
    if (getaddrinfo_result != 0) {
        fprintf(stderr, "getaddrinfo failed: %d\n", getaddrinfo_result);
        return 1;
    }

    printf("creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(
        bind_address->ai_family,
        bind_address->ai_socktype,
        bind_address->ai_protocol
    );

    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket listen failed: %d\n", SOCKET_ERROR_CODE);
        return 1;
    }

    int bind_result = bind(
        socket_listen,
        bind_address->ai_addr,
        bind_address->ai_addrlen
    );
    if (bind_result < 0) {
        fprintf(stderr, "bind failed: %d\n", SOCKET_ERROR_CODE);
        return 1;
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    int listen_result = listen(
        socket_listen,
        10 // backlog: the maximum length of the queue of pending connections
    );
    if (listen_result < SOCKET_ERROR_CODE) {
        fprintf(stderr, "listen failed: %d\n", SOCKET_ERROR_CODE);
        return 1;
    }

    // now the server has one socket that is listening for incoming connections
    // and may have multiple "client" sockets that are connected to clients
    while (true) {
        printf("waiting for connection...\n");
        struct sockaddr_storage client_address;
        socklen_t client_len = sizeof(client_address);
        SOCKET socket_client = accept(
            socket_listen,
            (struct sockaddr*) &client_address, 
            &client_len
        );
        if (!ISVALIDSOCKET(socket_client)) {
            fprintf(stderr, "accept() failed: %d\n", SOCKET_ERROR_CODE);
            return 1;
        }

        printf("client is connected\n");
        char address_buffer[1024];
        getnameinfo(
            (struct sockaddr*) &client_address,
            client_len,
            address_buffer, sizeof(address_buffer),
            0, 0,
            NI_NUMERICHOST
        );
        printf("Client IP: %s\n", address_buffer);

        int byte_send = 0;
        time_t timer;
        time(&timer);
        char *time_msg = ctime(&timer);
        byte_send = send(
            socket_client,
            time_msg,
            strlen(time_msg),
            0
        );
        if (byte_send < 0) {
            fprintf(stderr, "send() failed: %d\n", SOCKET_ERROR_CODE);
            return 1;
        }
        printf("sent bytes: %d\n", byte_send);
        
        printf("closing client socket...\n");
        CLOSESOCKET(socket_client);
    }
    printf("closing listen socket...\n");
    CLOSESOCKET(socket_listen);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#else
    // No cleanup needed for Unix/Linux
#endif
}