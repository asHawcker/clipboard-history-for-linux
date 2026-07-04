#include <stdio.h>
#include "ring_buffer.h"
#include "ipc_server.h"
#include <signal.h>

int main()
{
    ring_buffer_t rb;
    rb_init(&rb);
    signal(SIGPIPE, SIG_IGN);

    int server_fd = setup_secure_unix_socket();
    run_ipc_server(server_fd, &rb); // block forever so daemon keeps running

    return 0;
}