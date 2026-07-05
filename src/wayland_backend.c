#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>

// pointer to display connection
static struct wl_display *display = NULL;

int wayland_init(void)
{
    // connect to the wayland socket
    // I believe its /run/user/1000/wayland-0)
    display = wl_display_connect(NULL);
    if (!display)
    {
        fprintf(stderr, "[display] :: [ERROR] :: failed to connect to wayland display\n");
        return -1;
    }

    printf("[display] :: [INFO] :: connected to wayland display.\n");

    // get the underlying UNIX socket fd.
    int display_fd = wl_display_get_fd(display);

    return display_fd;
}

struct wl_display *wayland_get_display(void)
{
    return display;
}

void wayland_remove(void)
{
    if (display)
    {
        wl_display_disconnect(display);
        display = NULL;
    }
}