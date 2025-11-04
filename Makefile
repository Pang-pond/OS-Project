all:
	g++ -std=c++17 -pthread server.cpp -o server
	g++ -std=c++17 -pthread client.cpp -o client

clean:
	rm -f server client *.csv