#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include "ring_buffer.h"

// returns the file descriptor of the listening server socket
int setup_secure_unix_socket(void);

// handles incoming connections
void handle_client_connection(int server_fd, ring_buffer_t *rb, int uinput_fd);

#endif