#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>

#include "protocol.h"
#include "ipc_server.h"

int setup_secure_unix_socket(void)
{
    int server_fd;
    struct sockaddr_un addr;

    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir)
        runtime_dir = "/tmp";
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "%s%s", runtime_dir, CLIPD_SOCKET_PATH);

    // unlink dormant socket from previous crashes
    unlink(socket_path);

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    // apply strict umask 0077 so the socket file is created with 0600 permissions
    mode_t old_mask = umask(0077);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind error");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    umask(old_mask);

    if (listen(server_fd, 5) == -1)
    {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    printf("[clipd] :: [INFO] :: Listening securely on %s\n", socket_path);
    return server_fd;
}

void run_ipc_server(int server_fd, ring_buffer_t *rb)
{
    printf("[clipd] ;; [INFO] :: IPC Server ready to accept connections...\n");

    while (1)
    {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd == -1)
        {
            perror("[clipd] :: [ERROR] :: accept error");
            continue;
        }

        // verify credentials
        struct ucred cred;
        socklen_t cred_len = sizeof(struct ucred);
        if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) == -1)
        {
            perror("[clipd] :: [ERROR] :: credentials verification error");
            close(client_fd);
            continue;
        }

        if (cred.uid != getuid())
        {
            fprintf(stderr, "[clipd] :: [ERROR] :: unauthorized credentials UID: %d\n", cred.uid);
            close(client_fd);
            continue;
        }

        // read protocol header
        ipc_header_t header;
        ssize_t bytes_read = recv(client_fd, &header, sizeof(ipc_header_t), 0);

        if (bytes_read != sizeof(ipc_header_t))
        {
            fprintf(stderr, "[clipd] :: [WARN] :: Malformed packet or connection closed prematurely.\n");
            close(client_fd);
            continue;
        }

        if (header.uid != CLIPD_UID)
        {
            fprintf(stderr, "[clipd] :: [ERROR] :: Invalid UID (0x%X). Connection dropped.\n", header.uid);
            close(client_fd);
            continue;
        }

        // command execution
        switch (header.command)
        {
        case CMD_LIST_CLIPS:
            printf("[clipd] :: [INFO] :: CMD_LIST_CLIPS received. Current items: %d\n", rb->count);
            for (int i = 0; i < rb->count; i++)
            {
                int idx = (rb->tail + i) % MAX_CLIPS;
                printf("  -> Clip ID %u (Size: %zu bytes)\n", rb->entries[idx].id, rb->entries[idx].size);
            }
            break;
        case CMD_ADD_CLIP:
            printf("[clipd] :: [INFO] :: CMD_ADD_CLIP received. (Payload length: %u)\n", header.payload_len);

            if (header.payload_len == 0 || header.payload_len > MAX_PAYLOAD_SIZE)
            {
                fprintf(stderr, "[clipd] :: [ERROR] :: invalid payload.\n");
                break;
            }

            // temp space to receive data
            char *temp = malloc(header.payload_len);
            if (!temp)
            {
                fprintf(stderr, "[clipd] :: [ERROR] :: out of memory\n");
                break;
            }

            // revceive the data in the buffer
            ssize_t bytes_received = recv(client_fd, temp, header.payload_len, 0);
            if (bytes_received != (ssize_t)header.payload_len)
            {
                fprintf(stderr, "[clipd] :: [ERROR] :: socket read failed\n");
                free(temp);
                break;
            }

            // push to the queue
            uint32_t assigned_id = rb_push(rb, temp, header.payload_len);
            free(temp);
            printf("[clipd] :: [INFO] :: saved clip as ID %u.\n", assigned_id);

            ipc_header_t resp = {
                .uid = CLIPD_UID,
                .command = STATUS_OK,
                .payload_len = 0,
                .clip_id = assigned_id};

            send(client_fd, &resp, sizeof(resp), 0); // send OK status
            break;
        case CMD_GET_CLIP:
            printf("[clipd] :: [INFO] :: CMD_GET_CLIP received for ID %u.\n", header.clip_id);

            // query the queue
            const clip_entry_t *entry = rb_get(rb, header.clip_id);

            if (!entry || !entry->data)
            {
                fprintf(stderr, "[clipd] :: [WARN] :: Clip ID %u not found or expired.\n", header.clip_id);

                // response for error
                ipc_header_t err_resp = {
                    .uid = CLIPD_UID,
                    .command = STATUS_ERR,
                    .payload_len = 0,
                    .clip_id = header.clip_id};
                send(client_fd, &err_resp, sizeof(err_resp), 0);
                break;
            }

            // response for successful query
            ipc_header_t ok_resp = {
                .uid = CLIPD_UID,
                .command = STATUS_OK,
                .payload_len = entry->size,
                .clip_id = entry->id};

            // send header
            if (send(client_fd, &ok_resp, sizeof(ok_resp), 0) == -1)
            {
                perror("[clipd] :: [ERROR] :: Failed sending GET header response");
                break;
            }

            // send payload directly from the queue
            if (send(client_fd, entry->data, entry->size, 0) == -1)
            {
                perror("[clipd] :: [ERROR] :: Failed streaming raw clip payload");
                break;
            }
            printf("[clipd] :: [INFO] :: sent %zu bytes for ID %u to client.\n", entry->size, entry->id);
            break;
        default:
            fprintf(stderr, "[clipd] :: [ERROR] :: Unknown command received: %u\n", header.command);
            break;
        }

        close(client_fd);
    }
}