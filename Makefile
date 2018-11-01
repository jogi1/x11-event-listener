all:
	clang -g emit.c emit_x11.c emit_socket.c -lX11 -lXi -lpthread -DDEBUG -o event_listener
