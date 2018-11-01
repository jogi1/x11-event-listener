#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "emit.h"

int E_X11_init(void **ex_data);
int E_X11_start(struct emit *emit, void **ex_data);

int E_socket_init(struct emit *emit);
int E_socket_start(struct emit *emit);

int main(int argc, char **argv) {
    int rc;
    void *ex_data;

    struct emit *emit;

    if (argc < 2) {
	printf("you need to supply a socket path\n");
	return 1;
    }

    emit = (struct emit *)calloc(1, sizeof(struct emit ));
    if (emit == NULL) {
	printf("could not allocate emit\n");
    }

    emit->socket_path = argv[1];

    rc = E_X11_init(&ex_data);
    if (rc != 0)
    {
	return rc;
    }

    rc = E_socket_init(emit);
    if (rc != 0)
    {
	return rc;
    }


    emit->run = 1;

    E_X11_start(emit, ex_data);
    E_socket_start(emit);
    pthread_join(emit->connect, NULL);
    pthread_join(emit->send, NULL);
    pthread_join(emit->x11, NULL);
    return 0;
}
