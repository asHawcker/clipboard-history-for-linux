#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <wayland-client.h>

#include "ring_buffer.h"
#include "ipc_server.h"
#include "display.h"

extern struct wl_display *wayland_get_display(void);

int main()
{
    ring_buffer_t rb;
    rb_init(&rb);
    signal(SIGPIPE, SIG_IGN);

    int server_fd = setup_secure_unix_socket();

    session_type_t session = detect_session_type();
    int display_fd = init_display_listener(session);

    struct pollfd fds[2];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    fds[1].fd = display_fd;
    fds[1].events = (display_fd != -1) ? POLLIN : 0;

    // run_ipc_server(server_fd, &rb); // block forever so daemon keeps running

    while (1)
    {
        // flush out bound messages before sleeping
        if (session == SESSION_WAYLAND)
        {
            struct wl_display *wl_dspy = wayland_get_display();
            if (wl_dspy)
            {
                while (wl_display_prepare_read(wl_dspy) != 0)
                {
                    wl_display_dispatch_pending(wl_dspy);
                }
                wl_display_flush(wl_dspy);
            }
        }

        // sleep until a wake signal is recieved from the kernel
        int poll_ret = poll(fds, 2, -1);
        if (poll_ret < 0)
        {
            perror("[clipd] :: [ERROR] :: poll crash");
            break;
        }

        if (session == SESSION_WAYLAND)
        {
            struct wl_display *wl_dspy = wayland_get_display();
            if (fds[1].revents & POLLIN)
            {
                wl_display_read_events(wl_dspy);
                wl_display_dispatch_pending(wl_dspy);
            }
            else
            {
                wl_display_cancel_read(wl_dspy);
            }
        }

        // handle CLI client commands
        if (fds[0].revents & POLLIN)
        {
            handle_client_connection(server_fd, &rb);
        }

        // handle graphical selection updates
        if (display_fd != -1 && (fds[1].revents & POLLIN))
        {
            if (session == SESSION_WAYLAND)
            {
                printf("[clipd] :: [DEBUG] :: Wayland Display Protocol message received!\n");
                // TODO here... still testing
            }
        }
    }

    rb_free(&rb);
    close(server_fd);
    return 0;

    return 0;
}