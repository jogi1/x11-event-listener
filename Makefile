release:
	clang emit.c emit_x11.c emit_socket.c -lX11 -lXi -lxcb -lpthread -o event_listener -Wall -O3
	strip event_listener
debug:
	clang-tidy *.c
	clang -g emit.c emit_x11.c emit_socket.c -lX11 -lXi -lxcb -lpthread -DDEBUG -o event_listener -Wall
