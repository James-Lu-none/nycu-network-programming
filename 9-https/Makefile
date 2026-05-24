all: https_client https_server
https_server: http_server_to_https.c
	gcc -o https_server http_server_to_https.c -lssl -lcrypto
https_client: http_get_to_https.c
	gcc -o https_client http_get_to_https.c -lssl -lcrypto
clean:
	rm -f https_client https_server