release:
	clang emit.c emit_x11.c emit_socket.c -lX11 -lXi -lpthread -o event_listener -Wall -O3
	strip event_listener
debug:
	clang -g emit.c emit_x11.c emit_socket.c -lX11 -lXi -lpthread -DDEBUG -o event_listener -Wall
