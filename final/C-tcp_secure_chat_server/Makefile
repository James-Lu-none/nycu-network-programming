# src at ./src
# include at ./include

CC = g++
CFLAGS = -Wall -I./include

all: chat_server chat_client

chat_server: ./src/chat_server.cpp
	$(CC) $(CFLAGS) ./src/chat_server.cpp -o chat_server

chat_client: ./src/chat_client.cpp
	$(CC) $(CFLAGS) ./src/chat_client.cpp -o chat_client

clean:
	rm -f chat_server chat_client