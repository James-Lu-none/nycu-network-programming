#include "sock_compat.h"

const char *get_content_type(const char* path)
{
    char *extension[] = {
        ".css",
        ".csv",
        ".gif",
        ".htm",
        ".html",
        ".ico",
        ".jpeg",
        ".jpg",
        ".js",
        ".json",
        ".png",
        ".pdf",
        ".svg",
        ".txt"
    };

    char *mime_type[] = {
        "text/css",
        "text/csv",
        "image/gif",
        "text/html",
        "text/html",
        "image/x-icon",
        "image/jpeg",
        "image/jpeg",
        "application/javascript",
        "application/json",
        "image/png",
        "application/pdf",
        "image/svg+xml",
        "text/plain"
    };

    for (int i = 0; i < sizeof(extension) / sizeof(char*); i++)
    {
        if (strcmp(path + strlen(path) - strlen(extension[i]), extension[i]) == 0)
        {
            return mime_type[i];
        }
    }
    return "application/octet-stream";
}

SOCKET create_socket(const char* host, const char *port)
{
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *bind_address;
    getaddrinfo(host, port, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;

    socket_listen = socket(
        bind_address->ai_family,
        bind_address->ai_socktype,
        bind_address->ai_protocol
    );
    if (!ISVALIDSOCKET(socket_listen))
    {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    int option = 0;
    if (setsockopt(socket_listen, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&option, sizeof(option))) {
        fprintf(stderr, "setsockopt() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Binding socket to local address...\n");
    int bind_result = bind (
        socket_listen,
        bind_address->ai_addr,
        bind_address->ai_addrlen
    );

    if (bind_result)
    {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    freeaddrinfo(bind_address);

    printf("Listening...\n");
    int listen_result = listen(socket_listen, 10);
    if (listen_result < 0)
    {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen;
}

#define MAX_REQUEST_SIZE 2047
struct client_info
{
    SOCKET socket;
    struct sockaddr_storage address;
    socklen_t address_length;
    char request[MAX_REQUEST_SIZE + 1];
};

static struct client_info *client_list = 0;

struct client_info *get_client (SOCKET query_socket)
{
    struct client_info *client = client_list;
    while (client)
    {
        if (client->socket == query_socket || query_socket == -1)
        {
            return client;
        }
        client = (struct client_info*)client->request;
    }

    struct client_info *new_client = malloc(sizeof(struct client_info));
    new_client->socket = query_socket;
    new_client->address_length = sizeof(new_client->address);
    new_client->request[0] = 0;
    new_client->request[MAX_REQUEST_SIZE] = 0;

    new_client->request[0] = (char)client_list;
    client_list = new_client;

    return new_client;
}

void drop_client (struct client_info *to_drop_client)
{
    CLOSESOCKET(to_drop_client->socket);
}

const char *get_client_address(struct client_info *client)
{
    static char address_buffer[100];
    getnameinfo(
        (struct sockaddr*)&(client->address),
        client->address_length,
        address_buffer,
        sizeof(address_buffer),
        0,
        0,
        NI_NUMERICHOST
    );
    return address_buffer;
}

fd_set wait_on_clients(SOCKET server_socket)
{
    fd_set read_ready;
    FD_ZERO(&read_ready);
    FD_SET(server_socket, &read_ready);
    struct client_info *client = client_list;
    int max_socket = server_socket;
    while (client)
    {
        FD_SET(client->socket, &read_ready);
        if (client->socket > max_socket)
        {
            max_socket = client->socket;
        }
        client = (struct client_info*)client->request;
    }
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    int select_result = select(
        max_socket + 1,
        &read_ready,
        0,
        0,
        &timeout
    );
    if (select_result < 0)
    {
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    return read_ready;
}

void send_status_code
(
    struct client_info *to_send_client,
    int status_code,
    char *additional_header,
    char *additional_content
)
{
    char to_send_buffer[1024];
    memset(to_send_buffer, 0, sizeof(to_send_buffer));
    switch (status_code)
    {
    case 200:
        sprintf(to_send_buffer, "HTTP/1.1 200 OK\r\n");
        break;
    case 400:
        sprintf(to_send_buffer, "HTTP/1.1 400 Bad Request\r\n");
        break;
    default:
        sprintf(to_send_buffer, "HTTP/1.1 %d Unknown Status\r\n", status_code);
        break;
    }
    sprintf(to_send_buffer + strlen(to_send_buffer), "Content-Type: text/html\r\n");
    if (additional_header)
    {
        sprintf(to_send_buffer + strlen(to_send_buffer), "%s\r\n", additional_header);
    }
    sprintf(to_send_buffer + strlen(to_send_buffer), "\r\n");
    if (additional_content)
    {
        sprintf(to_send_buffer + strlen(to_send_buffer), "%s", additional_content);
    }

    send(to_send_client->socket, to_send_buffer, strlen(to_send_buffer), 0);
    drop_client(to_send_client);
}

#define RESPONSE_BUFFER_SIZE 1024
void send_resource
(
    struct client_info *client,
    const char *path
)
{
    char to_send_buffer[RESPONSE_BUFFER_SIZE + 1];
    memset(to_send_buffer, 0, sizeof(to_send_buffer));
    sprintf(to_send_buffer, "HTTP/1.1 200 OK\r\n");
    sprintf(to_send_buffer + strlen(to_send_buffer), "Content-Type: %s\r\n", get_content_type(path));
    sprintf(to_send_buffer + strlen(to_send_buffer), "\r\n");

    if (strcmp(path, "/") == 0)
    {
        path = "index.html";
    }

    if (strlen(path)>128) {
        send_status_code(client, 400, NULL, "Path too long.\r\n");
        return;
    }
    char full_path[256]; // 128 for path, 128 for "public/"
    sprintf(full_path, "public/%s", path);

    FILE *file = fopen(full_path, "rb");
    if (!file)
    {
        send_status_code(client, 400, NULL, "File not found.\r\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    size_t total_read = 0;
    while (total_read < file_size)
    {
        size_t to_read = file_size - total_read;
        if (to_read > RESPONSE_BUFFER_SIZE - strlen(to_send_buffer))
        {
            to_read = RESPONSE_BUFFER_SIZE - strlen(to_send_buffer);
        }
        size_t read = fread(to_send_buffer + strlen(to_send_buffer), 1, to_read, file);
        if (read <= 0)
        {
            break;
        }
        total_read += read;
    }

    send_status_code(client, 200, NULL, to_send_buffer);
}

int main(int argc, char** argv)
{

#if defined(_WIN32)
    WSADATA socket_data;
    if (WSAStartup(MAKEWORD(2, 2), &socket_data)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    SOCKET server_socket = create_socket(0, "8080");

    while (1)
    {
        fd_set read_ready;
        read_ready = wait_on_clients(server_socket);
        if (FD_ISSET(server_socket, &read_ready))
        {
            struct client_info *client = get_client(-1);
            client->socket = accept(
                server_socket,
                (struct sockaddr*) &(client->address),
                &(client->address_length)
            );

            if (!ISVALIDSOCKET(client->socket))
            {
                fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
                return 1;
            }

            printf("New Connection From %s. \n", get_client_address(client));
        }

        struct client_info *client = client_list;
        while (client)
        {
            /* Fill in the logic of client handling. */
        }
    }

    printf("\nClosing socket...\n");
    CLOSESOCKET(server_socket);

#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;
}