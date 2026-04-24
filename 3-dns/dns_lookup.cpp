#include "sock_compat.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage:\n\tdns_lookup hostanme\n");
        printf("Example:\n\tdns_lookup rdns.waynewolf.tw\n");
        exit(1);
    }

#if defined (_WIN32)
    WSADATA sock_data;
    if (WSAStartup(MAKEWORD(2, 2), &sock_data))
    {
        fprintf(stderr, "Failed to initialize Winsock.\n");
        return 1;
    }
#endif

    char *resolve_hostname = argv[1];
    printf("Resolving Hostname '%s'\n", resolve_hostname);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ALL;
    struct addrinfo *resolve_address;

    int getaddr_result = getaddrinfo(
        resolve_hostname,
        0,
        &hints,
        &resolve_address
    );

    if (getaddr_result)
    {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Remote Address is: ");
    struct addrinfo *address_it = resolve_address;
    while(address_it = address_it->ai_next) {
        char address_string[100];
        getnameinfo(
            address_it->ai_addr,
            address_it->ai_addrlen,
            address_string,
            sizeof(address_string),
            0, 0, NI_NUMERICHOST
        );
        printf("%s ", address_string);
    }
    printf("\n");

    freeaddrinfo(resolve_address);
    /**
     * Type your code here.
     **/

#if defined(_WIN32)
    WSACleanup();
#endif

    return 0;

}
