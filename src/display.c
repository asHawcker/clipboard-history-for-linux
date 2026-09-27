#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display.h"

static backend_type_t active_backend = BACKEND_NONE;

extern int wayland_init(void);
extern int wayland_get_fd(void);
extern void wayland_pre_poll(void);
extern void wayland_post_poll(struct pollfd *pfd, ring_buffer_t *rb);
extern int wayland_get_extra_pollfds(struct pollfd *fds, int max_fds);
extern void wayland_handle_extra_pollfds(struct pollfd *fds, int count, ring_buffer_t *rb);
extern void wayland_set_clipboard(const char *data, size_t size);
extern void wayland_cleanup(void);

extern int x11_init(void);
extern void x11_handle_event(ring_buffer_t *rb);
extern void x11_set_clipboard(const char *data, size_t size);
extern void x11_cleanup(void);
extern int x11_get_fd(void);

session_type_t detect_session_type(void)
{
    const char *session = getenv("XDG_SESSION_TYPE");
    if (session)
    {
        if (strcmp(session, "x11") == 0)
            return SESSION_X11;
        if (strcmp(session, "wayland") == 0)
            return SESSION_WAYLAND;
    }

    if (getenv("WAYLAND_DISPLAY"))
        return SESSION_WAYLAND;
    if (getenv("DISPLAY"))
        return SESSION_X11;

    return SESSION_UNKNOWN;
}

int display_init(void)
{
    session_type_t session = detect_session_type();

    if (session == SESSION_WAYLAND)
    {
        printf("[display] :: [INFO] :: Wayland session detected. Attempting native Wayland backend...\n");
        int fd = wayland_init();
        if (fd != -1)
        {
            active_backend = BACKEND_WAYLAND;
            printf("[display] :: [SUCCESS] :: Native Wayland backend active.\n");
            return fd;
        }

        printf("[display] :: [INFO] :: Native Wayland data-control unavailable. Attempting XWayland fallback...\n");
        wayland_cleanup(); // Clean up stale connection objects completely
    }

    // Attempt X11 / XWayland fallback
    printf("[display] :: [INFO] :: Initializing X11/XWayland backend...\n");
    int x11_fd = x11_init();
    if (x11_fd != -1)
    {
        active_backend = BACKEND_X11;
        printf("[display] :: [SUCCESS] :: X11/XWayland backend active.\n");
        return x11_fd;
    }

    fprintf(stderr, "[display] :: [ERROR] :: All display backends failed to initialize.\n");
    active_backend = BACKEND_NONE;
    return -1;
}

backend_type_t display_get_active_backend(void)
{
    return active_backend;
}

int display_get_fd(void)
{
    if (active_backend == BACKEND_WAYLAND)
        return wayland_get_fd();
    if (active_backend == BACKEND_X11)
        return x11_get_fd();
    return -1;
}

void display_pre_poll(void)
{
    if (active_backend == BACKEND_WAYLAND)
    {
        wayland_pre_poll();
    }
}

void display_post_poll(struct pollfd *pfd, ring_buffer_t *rb)
{
    if (active_backend == BACKEND_WAYLAND)
    {
        wayland_post_poll(pfd, rb);
    }
    else if (active_backend == BACKEND_X11)
    {
        if (pfd && (pfd->revents & POLLIN))
        {
            x11_handle_event(rb);
        }
    }
}

int display_get_extra_pollfds(struct pollfd *fds, int max_fds)
{
    if (active_backend == BACKEND_WAYLAND)
    {
        return wayland_get_extra_pollfds(fds, max_fds);
    }
    return 0;
}

void display_handle_extra_pollfds(struct pollfd *fds, int count, ring_buffer_t *rb)
{
    if (active_backend == BACKEND_WAYLAND)
    {
        wayland_handle_extra_pollfds(fds, count, rb);
    }
}

void display_set_clipboard(const char *data, size_t size)
{
    if (active_backend == BACKEND_WAYLAND)
    {
        wayland_set_clipboard(data, size);
    }
    else if (active_backend == BACKEND_X11)
    {
        x11_set_clipboard(data, size);
    }
    else
    {
        fprintf(stderr, "[display] :: [WARN] :: No active display backend to set clipboard.\n");
    }
}

void display_cleanup(void)
{
    if (active_backend == BACKEND_WAYLAND)
    {
        wayland_cleanup();
    }
    else if (active_backend == BACKEND_X11)
    {
        x11_cleanup();
    }
    active_backend = BACKEND_NONE;
}