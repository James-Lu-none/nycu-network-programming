all: time_client time_server time_server_http
time_client: time_client.cpp
	g++ -o time_client time_client.cpp
time_server: time_server.cpp
	g++ -o time_server time_server.cpp
time_server_http: time_server_http.cpp
	g++ -o time_server_http time_server_http.cpp
clean:
	rm -f time_client time_server time_server_http