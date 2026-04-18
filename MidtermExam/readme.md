# Midterm Exam

## design decisions

1. Modular Architecture:
    - `ChatServer` class (in `server.h`) encapsulates all server-side logic, including client management and message handling.
    - `BaseClient` (in `client.h`) with `TCPClient` and `UDPClient` subclasses provides a polymorphic interface basic operations.
    - Centralized logging (`logging.h`) and color tokens (`colors.h`) are used throughout the project.
2. heartbeat & lazy deletion:
    - client pings every 3 seconds and server responds with pong, server updates last_seen time
    - on client side, if server doesn't respond to ping with pong for 3 times, it will stop and exit
    - on server side, if last_seen time of a client is more than 10 seconds ago, it will be set to inactive for lazy deletion
    - for all disconnection events (including timeout, client quit, etc.), the client will be set to inactive instead of removed immediately
    - in the while loop, before select, iterate all clients and check for is_active, if inactive, remove it from all_clients
3. file descriptor management:
    - since select will modify the `fd_set`, we rebuild a `working_fds` bitmask every loop before select
    - in each rebuild, add `tcp_listen` socket (usually 3), `udp_socket` (usually 4), and all active clients' file descriptors to `working_fds` and update `max_fd`
4. tcp/udp connection handling:
    - for tcp, new connection is handled when `tcp_listen` fd is set, and new message is handled when specific client fd is set
    - for udp, new connection and message are both handled when `udp_socket` fd is set, and we determine if it's a new connection by checking if the source ip and port are already in `all_clients`
5. client name initialization:
    - for both tcp and udp, when a new client connects, assign a random username to it (Guest-{random_string})
    - since `your_name` is a required argment in the assignment, so i just send a `/nick` command to change the username to the one you provided when connected to the server

## additional features

1. Join/Leave Notifications: Server broadcasts system-wide notifications when clients connect or disconnect.
2. Color-Coded Output: 
   - Timestamps are gray.
   - Usernames are bold white.
   - Command responses are cyan.
   - Direct messages are magenta.
   - Protocol indicators are green (TCP) and yellow (UDP).
3. Enhanced Logging: Server console features timestamped logs with colored levels (`INFO`, `DEBUG`, `ERROR`).
4. Command Suite:
   - `/help`: Detailed command list.
   - `/dm <user> <msg>`: Private messaging.
   - `/list`: List all active users.
   - `/nick <name>`: Rename user.
   - `/quit`: Graceful disconnect.
   - `/ping`: Manual heartbeat update.

## observations

- when server reboots, all tcp client will be disconnected, but for udp clients, they will be reconnected and add to all_clients again due to the `/ping` command if its not exited yet, and it will get a new Guest-xxxxx username, although we can send `/nick` command to change the username to the one we provided when connected to the server, but i think its fine to leave it as it is to show what will happen when username isn't provided

## tmux test script

```bash
bash test.sh
```