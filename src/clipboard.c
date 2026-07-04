#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "protocol.h"

int connect_to_daemon()
{
    int client_fd;
    struct sockaddr_un addr;

    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir)
        runtime_dir = "/tmp";

    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "%s%s", runtime_dir, CLIPD_SOCKET_PATH);

    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("[clipboard] :: [ERROR] :: socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("[clipboard] :: [ERROR] :: connection error");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    return client_fd;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s add \"<payload text>\"\n", argv[0]);
        fprintf(stderr, "  %s list\n", argv[0]);
        fprintf(stderr, "  %s get <id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = connect_to_daemon();

    ipc_header_t header;
    memset(&header, 0, sizeof(header));
    header.uid = CLIPD_UID;

    if (strcmp(argv[1], "add") == 0 && argc >= 3)
    {
        header.command = CMD_ADD_CLIP;
        header.payload_len = strlen(argv[2]); // we need to exlude the NULL terminator to get raw data

        // send header
        if (send(fd, &header, sizeof(header), 0) == -1)
        {
            perror("[clipboard] :: [ERROR] :: send header error");
        }

        // send payload aka data
        if (send(fd, argv[2], header.payload_len, 0) == -1)
        {
            perror("[clipboard] :: [ERROR] :: send payload error");
        }
        printf("[clipboard] :: [INFO] :: sent CMD_ADD_CLIP (%u bytes)\n", header.payload_len);
    }
    else if (strcmp(argv[1], "list") == 0)
    {
        header.command = CMD_LIST_CLIPS;
        if (send(fd, &header, sizeof(header), 0) == -1)
        {
            perror("[clipboard] :: [ERROR] :: error sending data");
        }
        printf("[clipboard] :: [INFO] :: sent CMD_LIST_CLIPS\n");
    }
    else if (strcmp(argv[1], "get") == 0 && argc >= 3)
    {
        header.command = CMD_GET_CLIP;
        header.clip_id = (uint32_t)atoi(argv[2]);
        if (send(fd, &header, sizeof(header), 0) == -1)
        {
            perror("[clipboard] :: [ERROR] :: send error");
        }
        printf("[clipboard] :: [INFO] :: Sent CMD_GET_CLIP for ID %u\n", header.clip_id);
    }
    else
    {
        fprintf(stderr, "[clipboard] :: [ERROR] :: Invalid command.\n");
    }

    // For now we just close the socket Later we will block on recv() to get daemon responses. TODO
    close(fd);
    return EXIT_SUCCESS;
}