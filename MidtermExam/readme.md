# Midterm Exam

## design decisions

1. provide command handling to realize all functionalities and events
2. BaseClient class with virtual functions:
    - `TCPClient` and `UDPClient` inherit from `BaseClient` to have common attributes and methods
    - Smart pointer `shared_ptr` was used to store all active clients so we don't have to worry about memory management and object slicing
3. heartbeat mechanism:
    - client pings every 3 seconds and server responds with pong, server updates last_seen time
    - on client side, if server doesn't respond to ping with pong for 3 times, it will stop and exit
    - on server side, if last_seen time of a client is more than 10 seconds ago, it will be set to inactive for lazy deletion
4. lazy deletion:
    - for all disconnection events (including timeout, client quit, etc.), the client will be set to inactive instead of removed immediately
    - in the while loop, before select, iterate all clients and check for is_active, if inactive, remove it from all_clients
5. file descriptor management:
    - since select will modify the `fd_set`, we rebuild a `working_fds` bitmask every loop before select
    - in each rebuild, add `tcp_listen` socket (usually 3), `udp_socket` (usually 4), and all active clients' file descriptors to `working_fds` and update `max_fd`
6. tcp/udp connection handling:
    - for tcp, new connection is handled when `tcp_listen` fd is set, and new message is handled when specific client fd is set
    - for udp, new connection and message are both handled when `udp_socket` fd is set, and we determine if it's a new connection by checking if the source ip and port are already in `all_clients`
7. client name initialization:
    - for both tcp and udp, when a new client connects, assign a random username to it (Guest-{random_string})
    - since `your_name` is a required argment in the assignment, so i just send a `/nick` command to change the username to the one you provided when connected to the server
8. `>` idicates that the message is fron user keyboard input, `<` idicates that the message is from server, yellow text indicates that its a command response

## additional features

1. `/help` command to show all commands.
2. `/dm` command to support peer-to-peer private communication besides the public broadcast.
3. `/list` command to view all active tcp and udp clients.
4. `/nick` command to change nickname.
5. `/quit` command to quit the server.
6. `/ping` command to update last seen time. (only logs on server side, quiet on client side)
7. ANSI color codes to colorize the output.
8. centralized logging system to log all events and messages.

## observations

1. when server reboots, all tcp client will be disconnected, but for udp clients, they will be reconnected and add to all_clients again due to the `/ping` command if its not exited yet, and it will get a new Guest-xxxxx username, although we can send `/nick` command to change the username to the one we provided when connected to the server, but i think its fine to leave it as it is to show what will happen when username isn't provided
2. 

## tmux test script

```bash
bash test.sh
```