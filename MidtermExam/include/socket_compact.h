// if multiple clients connect to the server, the server will create a new socket for each client socket_socket[n]
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