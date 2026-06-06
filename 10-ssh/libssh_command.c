// implement ssh client

#include <stdio.h>
#include <stdlib.h>
#include <libssh/libssh.h>

int main(int argc, char *argv[])
{
    // arguments: ssh_client <hostname>@<username>
    // additional arguments: -p <port>
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <hostname>@<username>\n", argv[0]);
        exit(-1);
    }
    char *hostname = argv[1];
    char *username = strchr(hostname, '@');
    // read port number from arguments
    if (username == NULL)
    {
        fprintf(stderr, "Invalid hostname: %s\n", hostname);
        exit(-1);
    }
    *username = '\0';
    username++;
    username = strchr(username, '@');
    // read port number from arguments

    ssh_session my_ssh_session;
    int rc;

    // create session and set options
    my_ssh_session = ssh_new();
    if (my_ssh_session == NULL)
        exit(-1);
    ssh_options_set(my_ssh_session, SSH_OPTIONS_HOST, hostname);

    // connect to server
    rc = ssh_connect(my_ssh_session);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Error connecting to %s: %s\n", hostname, ssh_get_error(my_ssh_session));
        exit(-1);
    }

    // authenticate ourselves
    rc = ssh_userauth_password(my_ssh_session, username, "password");
    if (rc != SSH_AUTH_SUCCESS)
    {
        fprintf(stderr, "Error authenticating with password: %s\n",
                ssh_get_error(my_ssh_session));
        exit(-1);
    }

    // execute a command
    ssh_channel channel;
    channel = ssh_channel_new(my_ssh_session);
    if (channel == NULL)
        exit(-1);
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK)
        exit(-1);
    rc = ssh_channel_request_exec(channel, "ls -l");
    if (rc != SSH_OK)
        exit(-1);

    // read the output of the command
    char buffer[256];
    int nbytes;
    nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    while (nbytes > 0)
    {
        fwrite(buffer, 1, nbytes, stdout);
        nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    }

    // close the channel and session
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(my_ssh_session);
    ssh_free(my_ssh_session);

    return 0;
}