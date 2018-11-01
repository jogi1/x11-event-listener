enum read_state {rs_write=0, rs_send};
struct emit_socket {
    int invalid;
    int socket;
};
struct emit {
    char buffer[2048];
    int write_length;
    enum read_state read_state;
    int run;
    void *ex_data;
    int socket;
    pthread_t x11, connect, send;
    struct emit_socket *connections;
    int connections_count;
    pthread_mutex_t socket_lock, buffer_lock;
    char *socket_path;
};
