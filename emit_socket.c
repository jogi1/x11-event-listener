#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>

#include "emit.h"

int E_socket_init(struct emit *emit) {
    struct sockaddr_un local;
    int len;

    emit->socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (emit->socket == -1) {
	fprintf(stderr,"could not open socket");
	return 1;
    }

    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, emit->socket_path, sizeof(local.sun_path));
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(emit->socket, (struct sockaddr *)&local, len) == -1) {
        fprintf(stderr, "bind");
	return 1;
    }

    if (listen(emit->socket, 5) == -1) {
        fprintf(stderr, "lsten");
	return 1;
    }

    return 0;
}

void *E_socket_handle_connect(void *ptr) {
    struct emit *emit;
    emit = (struct emit *)ptr;
    int s;
    unsigned int t;
    int i, viable_connections;

    struct sockaddr remote;

    struct emit_socket *es;

    printf("socket connect: started\n");
    while (emit->run) {
	if ((s = accept(emit->socket, &remote, &t)) != 0) {
	    pthread_mutex_lock(&emit->socket_lock);
	    printf("acceptiong connection\n");
	    for (i=0, viable_connections=0;i<emit->connections_count;i++){
		if (emit->connections[i].invalid == 0) {
		    viable_connections++;
		}
	    }
	    es = (struct emit_socket *)calloc(viable_connections + 1, sizeof(struct emit_socket));
	    if (es == NULL) {
		emit->run = 0;
		return NULL;
	    }

	    for (i=0, viable_connections=0;i<emit->connections_count;i++){
		if (emit->connections[i].invalid == 0) {
		    memcpy((void *)&es[viable_connections], (void *)&emit->connections[i], sizeof(struct emit_socket));
		    viable_connections++;
		}
	    }
	    free(emit->connections);
	    emit->connections = es;
	    emit->connections_count = viable_connections + 1;
	    emit->connections[viable_connections].socket = s;
	    pthread_mutex_unlock(&emit->socket_lock);

	}
    }

    printf("closing socket_handle_connect\n");
    return NULL;
}

void E_socket_handle_send(void *ptr) {
    struct emit *emit;
    emit = (struct emit *)ptr;

    printf("socket connect: send\n");
    while (emit->run) {
	if (emit->read_state == rs_send) {
	    pthread_mutex_lock(&emit->socket_lock);
	    pthread_mutex_unlock(&emit->socket_lock);
	    emit->read_state = rs_write;
	}
    }

    printf("closing socket_handle_send\n");
    return;
}



int E_socket_start(struct emit *emit) {
    if (pthread_create(&emit->connect, NULL, &E_socket_handle_connect, (void *)emit) != 0) {
	printf("could not created connect thread\n");
	return 1;
    }
    /*
    if (pthread_create(&emit->send, NULL, E_socket_handle_send, (void *)emit) != 0) {
	printf("could not created connect thread\n");
	return 1;
    }
    */
    return 0;
}
