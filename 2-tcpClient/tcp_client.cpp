#include "../socket_compact.h"
#include <iostream>
#include <cstring>
#include <string>
#include <time.h>
#include <stdio.h>
// socket bind listen accept send close in multiple threads, so the server can handle multiple clients at the same time

#ifdef _WIN32
    #include <conio.h>
#endif
SOCKET socket_1;
SOCKET socket_2;
SOCKET socket_3;

using namespace std;
int main(int argc, char* argv[]) {
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

    char *dest_host = argv[1];
    char *dest_port = argv[2];

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* peer_address;
    int getaddrinfo_result = getaddrinfo(
        dest_host, // hostname
        dest_port, // service (port)
        &hints, // hints that tells the type of socket we want
        &peer_address // the peer_address struct to hold the server address
    );
    
    
    if (getaddrinfo_result != 0) {
        fprintf(stderr, "getaddrinfo failed: %d\n", getaddrinfo_result);
        return 1;
    }

    char dest_addr_buffer[100];
    char dest_port_buffer[100];
    getnameinfo(
        peer_address->ai_addr,
        peer_address->ai_addrlen,
        dest_addr_buffer, sizeof(dest_addr_buffer),
        dest_port_buffer, sizeof(dest_port_buffer),
        NI_NUMERICHOST
    );
    printf("Remote Host: %s\n", dest_addr_buffer);
    printf("Remote Port: %s\n", dest_port_buffer);

    printf("Creating socket...\n");
    SOCKET socket_to_dest;
    socket_to_dest = socket(
        peer_address->ai_family,
        peer_address->ai_socktype,
        peer_address->ai_protocol
    );

    if (!ISVALIDSOCKET(socket_to_dest)) {
        fprintf(stderr, "socket failed: %d\n", SOCKET_ERROR_CODE);
        return 1;
    }
    printf("Connecting...\n");
    int connect_result = connect(
        socket_to_dest,
        peer_address->ai_addr,
        peer_address->ai_addrlen
    );
    if (connect_result < 0) {
        fprintf(stderr, "connect failed: %d\n", SOCKET_ERROR_CODE);
        return 1;
    }

    freeaddrinfo(peer_address);

    while (1) {
        fd_set read_ready;
        FD_ZERO(&read_ready);
        FD_SET(socket_to_dest, &read_ready);

        #if !defined(_WIN32) && !defined(_WIN64)
        FD_SET(fileno(stdin), &read_ready);
        #endif

        struct timeval select_timeout;
        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = 100000; // 100 milliseconds

        // select() is a system call that allows a program to monitor multiple file descriptors (sockets) for events such as incoming connections or data to read
        // the reason to use select() here is to allow the client to read from the socket and also check for user input (e.g., to exit the program) without blocking on either operation
        int select_result = select(
            socket_to_dest + 1, // nfds: the highest-numbered file descriptor in any of the three sets, plus 1
            &read_ready, // readfds: the set of file descriptors to be checked for readability
            NULL, // writefds: the set of file descriptors to be checked for writability
            NULL, // exceptfds: the set of file descriptors to be checked for errors
            &select_timeout // timeout: the maximum interval to wait for any file descriptor to become ready
        );

        if (select_result < 0) {
            fprintf(stderr, "select failed: %d\n", SOCKET_ERROR_CODE);
            return 1;
        }

        if (FD_ISSET(socket_to_dest, &read_ready)) {
            char recv_buffer[4096];
            int bytes_received = recv(
                socket_to_dest,
                recv_buffer,
                sizeof(recv_buffer) - 1,
                0
            );
            if (bytes_received < 0) {
                fprintf(stderr, "recv failed: %d\n", SOCKET_ERROR_CODE);
                return 1;
            } else if (bytes_received == 0) {
                printf("Connection closed by peer\n");
                break;
            } else {
                recv_buffer[bytes_received] = '\0';
                printf("Received: %s\n", recv_buffer);
            }
        }

        // handle user input and send data to the server if needed (e.g., to exit the program)
        #if defined(_WIN32)
        if (_kbhit())
        #else
        if (FD_ISSET(fileno(stdin), &read_ready))
        #endif
        {
            char input_buffer[4096];
            if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
                fprintf(stderr, "Error reading input\n");
                break;
            }
            printf("Sending: %s", input_buffer);
            int bytes_sent = send(
                socket_to_dest, 
                input_buffer,
                strlen(input_buffer),
                0
            );
            if (bytes_sent < 0) {
                fprintf(stderr, "send failed: %d\n", SOCKET_ERROR_CODE);
                return 1;
            }
            // if the user types "exit", we can break the loop and close the connection
            if (strncmp(input_buffer, "exit", 4) == 0) {
                printf("Exiting...\n");
                break;
            }
        }
    }
    printf("Closing socket...\n");
    CLOSESOCKET(socket_to_dest);

    #if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
    #else
        // No cleanup needed for Unix/Linux
    #endif
    printf("Client program terminated.\n");
    return 0;
}