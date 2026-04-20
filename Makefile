all: clean server client

server: server.cpp lib/types.hpp lib/functions.hpp lib/define.hpp
	g++ server.cpp -o server -lpthread

client: client.cpp lib/types.hpp lib/functions.hpp lib/define.hpp lib/player.hpp modules/miniaudio.h
	g++ client.cpp -o client -lpthread

clean:
	touch server client
	rm server client