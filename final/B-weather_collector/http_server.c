#include "sock_compat.h"

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
        exit(1);
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

typedef struct
{
    SOCKET socket;
    struct sockaddr_storage address;
    socklen_t address_length;
    char request[2048];
    int received;
    client_info *next;
} client_info;

static client_info *client_list = 0;

client_info *get_client (SOCKET query_socket)
{
    client_info *client = client_list;
    while (client)
    {
        if (client->socket == query_socket)
        {
            return client;
        }
        client = client->next;
    }

    client_info *new_client = malloc(sizeof(client_info));
    new_client->socket = query_socket;
    new_client->address_length = sizeof(new_client->address);
    new_client->request[0] = 0;
    new_client->received = 0;
    new_client->next = client_list;
    client_list = new_client;

    return new_client;
}

void drop_client (client_info *to_drop_client)
{
    CLOSESOCKET(to_drop_client->socket);
    client_info **pp = &client_list;
    while (*pp)
    {
        if (*pp == to_drop_client)
        {
            *pp = to_drop_client->next;
            free(to_drop_client);
            return;
        }
        pp = &((*pp)->next);
    }
}

const char *get_client_address(client_info *client)
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
    client_info *client = client_list;
    int max_socket = server_socket;
    while (client)
    {
        FD_SET(client->socket, &read_ready);
        if (client->socket > max_socket)
        {
            max_socket = client->socket;
        }
        client = client->next;
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

void send_status_code(client_info *to_send_client, int status_code, const char *body)
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
    sprintf(to_send_buffer + strlen(to_send_buffer), "\r\n");
    if (body)
    {
        sprintf(to_send_buffer + strlen(to_send_buffer), "%s", body);
    }

    send(to_send_client->socket, to_send_buffer, strlen(to_send_buffer), 0);
    drop_client(to_send_client);
}

int handle_weather_data(float new_temp, float new_hum)
{
    FILE *file = fopen("weather.csv", "r");
    if (!file)
    {
        return 0; // File does not exist, so it's not a duplicate
    }

    char line[256];
    char last_line[256] = {0};
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0')
        {
            strcpy(last_line, line);
        }
    }
    fclose(file);

    if (last_line[0] == '\0')
    {
        return 0; // File is empty
    }
    
    char *last_comma = strrchr(last_line, ',');
    if (!last_comma) return 0;
    float last_hum = (float)atof(last_comma + 1);

    *last_comma = '\0';
    char *prev_comma = strrchr(last_line, ',');
    if (!prev_comma) return 0;
    float last_temp = (float)atof(prev_comma + 1);

    return (last_temp == new_temp && last_hum == new_hum);
}

void handle_http_request(client_info *client, char *body, int content_length)
{
    body[content_length] = '\0';

    char method[16] = {0};
    char path[256] = {0};
    if (sscanf(client->request, "%15s %255s", method, path) != 2)
    {
        send_status_code(client, 400, "Bad request line.\r\n");
        return;
    }

    printf("Request - Method: %s, Path: %s\n", method, path);

    if (strcmp(method, "POST") == 0)
    {
        float temp = 0.0f;
        float hum = 0.0f;
        // Parse input format: <temperature>, <humidity>
        if (sscanf(body, "%f , %f", &temp, &hum) == 2)
        {
            if (handle_weather_data(temp, hum)) {
                send_status_code(client, 200, "Duplicate data received. Ignoring.\r\n");
            } else {
                send_status_code(client, 200, "Data received successfully.\r\n");
            }
        }
        else
        {
            send_status_code(client, 400, "Invalid data format. Expected: <temperature>, <humidity>\r\n");
        }
    }
    else
    {
        send_status_code(client, 400, "Method not supported.\r\n");
    }
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
            client_info *client = get_client(-1);
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

        client_info *client = client_list;
        while (client)
        {
            client_info *next = client->next;
            if (FD_ISSET(client->socket, &read_ready))
            {
                if (client->received >= MAX_REQUEST_SIZE)
                {
                    send_status_code(client, 400, "Request too large.\r\n");
                    client = next;
                    continue;
                }

                int bytes_received = recv(
                    client->socket,
                    client->request + client->received,
                    MAX_REQUEST_SIZE - client->received,
                    0
                );

                if (bytes_received <= 0)
                {
                    printf("Client disconnected or read error.\n");
                    drop_client(client);
                }
                else
                {
                    client->received += bytes_received;
                    client->request[client->received] = '\0';

                    char *header_end = strstr(client->request, "\r\n\r\n");
                    if (!header_end)
                    {
                        header_end = strstr(client->request, "\n\n");
                    }

                    if (header_end)
                    {
                        char *body = header_end + (header_end[0] == '\r' ? 4 : 2);

                        int content_length = 0;
                        char *cl_header = strstr(client->request, "Content-Length:");
                        if (!cl_header)
                        {
                            cl_header = strstr(client->request, "content-length:");
                        }
                        if (cl_header)
                        {
                            content_length = atoi(cl_header + 15);
                        }

                        int header_len = body - client->request;
                        if (client->received >= header_len + content_length)
                        {
                            handle_http_request(client, body, content_length);
                        }
                    }
                }
            }
            client = next;
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