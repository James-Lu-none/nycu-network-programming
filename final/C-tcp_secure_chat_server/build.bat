gcc -I ./include ./src/chat_server.c -o chat_server -lssl -lcrypto -lws2_32
gcc -I ./include ./src/chat_client.c -o chat_client -lssl -lcrypto -lws2_32