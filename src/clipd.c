#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include "ring_buffer.h"
#include "uinput_backend.h"
#include "ipc_server.h"
#include "display.h"

int main()
{
    ring_buffer_t rb;
    rb_init(&rb);
    signal(SIGPIPE, SIG_IGN);

    int server_fd = setup_secure_unix_socket();
    int display_fd = display_init();

    int uinput_fd = uinput_init();

    while (1)
    {
        display_pre_poll();

        struct pollfd fds[8];
        int nfds = 0;

        // Index 0: IPC Server Socket
        fds[0].fd = server_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        int ipc_idx = 0;
        nfds++;

        // Index 1: Display Server Socket (if active)
        int display_idx = -1;
        if (display_fd != -1)
        {
            fds[nfds].fd = display_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            display_idx = nfds++;
        }

        int extra_start_idx = nfds;
        int extra_count = display_get_extra_pollfds(&fds[nfds], 8 - nfds);
        nfds += extra_count;

        int timeout = -1;
        if (display_get_active_backend() == BACKEND_X11)
        {
            if (x11_has_pending_events())
            {
                timeout = 0;
            }
        }

        int poll_ret = poll(fds, nfds, timeout);
        if (poll_ret < 0)
        {
            perror("[clipd] :: [ERROR] :: poll crash");
            break;
        }

        // 1. Post-poll display processing
        if (display_idx != -1)
        {
            display_post_poll(&fds[display_idx], &rb);
        }

        // 2. Process asynchronous pipe transfer events
        if (extra_count > 0)
        {
            display_handle_extra_pollfds(&fds[extra_start_idx], extra_count, &rb);
        }

        // 3. Process CLI client IPC commands
        if (fds[ipc_idx].revents & POLLIN)
        {
            handle_client_connection(server_fd, &rb, uinput_fd);
        }
    }

    rb_free(&rb);
    close(server_fd);
    display_cleanup();
    uinput_cleanup(uinput_fd);
    return 0;
}