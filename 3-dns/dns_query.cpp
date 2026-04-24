#include "sock_compat.h"

const char *print_name(
    const unsigned char *dns_message,
    const unsigned char *current_pointer,
    const unsigned char *end_of_message
) {
    static char name_buffer[1024];
    unsigned char *out = (unsigned char *)name_buffer;
    const unsigned char *p = current_pointer;
    int jumped = 0;
    int offset;
    int first_pass_length = 0;

    *out = '\0';

    while (*p) {
        if ((*p & 0xC0) == 0xC0) {
            offset = ((*p & 0x3F) << 8) | *(p + 1);
            p = dns_message + offset;
            if (!jumped) first_pass_length += 2;
            jumped = 1;
        } else {
            int len = *p;
            if (p + len + 1 > end_of_message) return NULL;
            p++;
            if (out != (unsigned char *)name_buffer) *out++ = '.';
            memcpy(out, p, len);
            out += len;
            p += len;
            if (!jumped) first_pass_length += (len + 1);
        }
        if (out - (unsigned char *)name_buffer > 1022) break;
    }
    *out = '\0';
    return name_buffer;
}

const unsigned char* skip_name(const unsigned char* p) {
    while (*p) {
        if ((*p & 0xC0) == 0xC0) {
            return p + 2;
        }
        p += (*p + 1);
    }
    return p + 1;
}
void print_dns_message(const char *dns_message, int dns_message_length)
{
    if (dns_message_length < 12) {
        fprintf(stderr, "DNS message too short.\n");
        return;
    }

    const unsigned char *header_pointer = (const unsigned char *)dns_message;
    const unsigned char *end_of_message = header_pointer + dns_message_length;

    printf("\n--- DNS Header Parsing ---\n");
    printf("Transaction ID: 0x%02x%02x\n", header_pointer[0], header_pointer[1]);

    const int qr = (header_pointer[2] & 0x80) >> 7;
    const int opcode = (header_pointer[2] & 0x78) >> 3;
    const int aa = (header_pointer[2] & 0x04) >> 2;
    const int tc = (header_pointer[2] & 0x02) >> 1;
    const int rd = (header_pointer[2] & 0x01);
    const int ra = (header_pointer[3] & 0x80) >> 7;
    const int rcode = header_pointer[3] & 0x0F;

    printf("QR: %d (%s)\n", qr, qr ? "Response" : "Query");
    
    printf("OpCode: %d (", opcode);
    switch (opcode) {
        case 0: printf("Standard Query"); break;
        case 1: printf("Inverse Query"); break;
        case 2: printf("Server Status Request"); break;
        case 4: printf("Notify"); break;
        case 5: printf("Update"); break;
        default: printf("Reserved/Unknown"); break;
    }
    printf(")\n");

    printf("AA: %d, TC: %d, RD: %d, RA: %d\n", aa, tc, rd, ra);

    printf("RCode: %d (", rcode);
    switch (rcode) {
        case 0: printf("No Error"); break;
        case 1: printf("Format Error"); break;
        case 2: printf("Server Failure"); break;
        case 3: printf("NXDomain - Name Error"); break;
        case 4: printf("Not Implemented"); break;
        case 5: printf("Refused"); break;
        default: printf("Other/Reserved"); break;
    }
    printf(")\n");

    int qdcount = (header_pointer[4] << 8) | header_pointer[5];
    int ancount = (header_pointer[6] << 8) | header_pointer[7];
    int nscount = (header_pointer[8] << 8) | header_pointer[9];
    int arcount = (header_pointer[10] << 8) | header_pointer[11];
    printf("Counts: Questions: %d, Answers: %d, Authority: %d, Additional: %d\n", qdcount, ancount, nscount, arcount);

    const unsigned char *content_ptr = header_pointer + 12;

    for (int i = 0; i < qdcount; i++) {
        if (content_ptr >= end_of_message) break;
        printf("\n[Question %d]\n", i + 1);
        const char *name = print_name(header_pointer, content_ptr, end_of_message);
        printf("Name: %s\n", name ? name : "(parsing error)");
        
        content_ptr = skip_name(content_ptr);
        int type = (content_ptr[0] << 8) | content_ptr[1];
        int class_ = (content_ptr[2] << 8) | content_ptr[3];
        
        printf("  Type: %d (", type);
        switch(type) {
            case 1: printf("A"); break;
            case 2: printf("NS"); break;
            case 5: printf("CNAME"); break;
            case 15: printf("MX"); break;
            case 16: printf("TXT"); break;
            case 28: printf("AAAA"); break;
            case 255: printf("ANY"); break;
            default: printf("Unknown"); break;
        }
        printf("), Class: %d (IN)\n", class_);
        content_ptr += 4;
    }
    for (int i = 0; i < ancount; i++) {
        if (content_ptr >= end_of_message) break;
        printf("\n[Answer %d]\n", i + 1);
        const char *name = print_name(header_pointer, content_ptr, end_of_message);
        printf("  Name: %s\n", name);
        
        content_ptr = skip_name(content_ptr);
        int type = (content_ptr[0] << 8) | content_ptr[1];
        int rdlen = (content_ptr[8] << 8) | content_ptr[9];
        const unsigned char *rdata = content_ptr + 10;

        printf("  Type: %d, Length: %d\n", type, rdlen);
        printf("  Data: ");
        if (type == 1 && rdlen == 4) { // IPv4
            printf("%d.%d.%d.%d\n", rdata[0], rdata[1], rdata[2], rdata[3]);
        } else if (type == 28 && rdlen == 16) { // IPv6
            for (int j = 0; j < 16; j+=2) printf("%02x%02x%c", rdata[j], rdata[j+1], j==14?' ' : ':');
            printf("\n");
        } else if (type == 5 || type == 2) { // CNAME or NS
            printf("%s\n", print_name(header_pointer, rdata, end_of_message));
        } else {
            printf("(Hex) ");
            for(int j=0; j<rdlen; j++) printf("%02x ", rdata[j]);
            printf("\n");
        }
        content_ptr += 10 + rdlen;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        printf("Usage:\n\tdns_query dns_server hostname type\n");
        printf("Example:\n\tdns_query rdns.waynewolf.tw www.nycu AAAA\n");
        exit(0);
    }

    if (strlen(argv[1]) > 255)
    {
        fprintf(stderr, "DNS Server Name too long.\n");
        exit(1);
    }
    else if (strlen(argv[2]) > 255)
    {
        fprintf(stderr, "Hostname too long.\n");
        exit(1);
    }

    char *dns_server = argv[1];
    char *host_to_resolve = argv[2];
    char type;
    if (strcmp(argv[3], "A") == 0) {
        type = 1;
    } else if (strcmp(argv[3], "MX") == 0) {
        type = 15;
    } else if (strcmp(argv[3], "TXT") == 0) {
        type = 16;
    } else if (strcmp(argv[3], "AAAA") == 0) {
        type = 28;
    } else if (strcmp(argv[3], "ANY") == 0) {
        type = 255;
    } else {
        fprintf(stderr, "Unknown type '%s'. Use A, AAAA, TXT, MX, or ANY.",
                argv[3]);
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

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo *dns_address;
    int getaddr_result = getaddrinfo(
        dns_server,
        "53",
        &hints,
        &dns_address
    );

    if (getaddr_result)
    {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Creating socket...\n");
    SOCKET socket_to_dns;
    socket_to_dns = socket(
        dns_address->ai_family,
        dns_address->ai_socktype,
        dns_address->ai_protocol
    );

    if (!ISVALIDSOCKET(socket_to_dns))
    {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /* Start Forming your DNS Query Here. */

    char query[1024] = {
        0x00, 0x01, // Transaction ID
        0x01, 0x00, // Flags
        0x00, 0x01, // Questions
        0x00, 0x00, // Answer RRs
        0x00, 0x00, // Authority RRs
        0x00, 0x00, // Additional RRs
    };

    char *message_pointer = query + 12;
    const char *temp_host = host_to_resolve; 

    while (*temp_host)
    {
        char *label_length_pointer = message_pointer++;
        int len = 0;
        while (*temp_host && *temp_host != '.')
        {
            *message_pointer++ = *temp_host++;
            len++;
        }
        *label_length_pointer = len;
        if (*temp_host == '.')
            temp_host++;
    }
    *message_pointer++ = 0; // Terminating null byte
    *message_pointer++ = 0; // Type high byte
    *message_pointer++ = type; // Type low byte
    *message_pointer++ = 0; // Class high byte
    *message_pointer++ = 1; // Class low byte (IN)

    int query_size = message_pointer - query;

    int bytes_sent = sendto(
        socket_to_dns,
        query,
        query_size,
        0,
        dns_address->ai_addr,
        dns_address->ai_addrlen
    );
    printf("Sent %d bytes.\n", bytes_sent);
    print_dns_message(query, query_size);

    char received[1024];
    int bytes_received = recvfrom(
        socket_to_dns,
        received,
        1024,
        0,
        0,
        0
    );
    printf("Received %d bytes.\n", bytes_received);
    print_dns_message(received, bytes_received);
    printf("\n");

    freeaddrinfo(dns_address);
    CLOSESOCKET(socket_to_dns);

#if defined(_WIN32)
    WSACleanup();
#endif

    return 0;
}