CC = g++
CFLAGS = -Wall

all: chat_server chat_client

chat_server: chat_server.cpp socket_compact.h client.h
	$(CC) $(CFLAGS) chat_server.cpp -o chat_server

chat_client: chat_client.cpp socket_compact.h client.h
	$(CC) $(CFLAGS) chat_client.cpp -o chat_client

clean:
	rm -f chat_server chat_client