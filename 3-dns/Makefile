all: dns_lookup dns_query
dns_lookup: dns_lookup.cpp
	g++ -o dns_lookup dns_lookup.cpp
dns_query: dns_query.cpp
	g++ -o dns_query dns_query.cpp
clean:
	rm -f dns_lookup dns_query