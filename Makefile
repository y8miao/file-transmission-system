all:
	g++ -std=c++11 server.cc -o server
	g++ -std=c++11 client.cc -o client
clean:
	rm server client port 
