#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>

#include "emit.h"

int E_X11_init(void **ex_data);
int E_X11_start(struct emit *emit, void **ex_data);

int E_socket_init(struct emit *emit);
int E_socket_start(struct emit *emit);

int
main()
{
    int rc;
    void *ex_data;

    struct emit *emit;

    emit = (struct emit *)calloc(1, sizeof(struct emit ));
    if (emit == NULL) {
	printf("could not allocate emit\n");
    }

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
    sleep(5);
    pthread_join(emit->connect, NULL);
    pthread_join(emit->send, NULL);
    pthread_join(emit->x11, NULL);
    return 1;
}
