all: clean server client

server: server.cpp types.hpp functions.hpp define.hpp
	g++ server.cpp -o server -lpthread

client: client.cpp types.hpp functions.hpp define.hpp player.hpp modules/miniaudio.h
	g++ client.cpp -o client -lpthread

clean:
	rm server client