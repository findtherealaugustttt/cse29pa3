all: chat-server

chat-server: http-server.c data_handle.c
	gcc -std=c99 -g http-server.c data_handle.c -o chat-server

clean:
	rm -f chat-server
