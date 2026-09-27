#ifndef DISPLAY_H
#define DISPLAY_H

#include <poll.h>
#include <stddef.h>
#include "ring_buffer.h"

typedef enum {
    SESSION_UNKNOWN = 0,
    SESSION_X11,
    SESSION_WAYLAND
} session_type_t;

typedef enum {
    BACKEND_NONE = 0,
    BACKEND_X11,
    BACKEND_WAYLAND
} backend_type_t;

session_type_t detect_session_type(void);

// initializes active display backend with fallback
int display_init(void);

// returns active backend type
backend_type_t display_get_active_backend(void);

// Returns main display socket FD for poll()
int display_get_fd(void);

// pre-poll display processing
void display_pre_poll(void);

// Post-poll display processing
void display_post_poll(struct pollfd *pfd, ring_buffer_t *rb);

// returns any additional FDs such as Wayland pipe transfers for poll()
int display_get_extra_pollfds(struct pollfd *fds, int max_fds);

// processes events on additional FDs
void display_handle_extra_pollfds(struct pollfd *fds, int count, ring_buffer_t *rb);

// display-server-neutral clipboard ownership setter
void display_set_clipboard(const char *data, size_t size);

// Clean up display resources
void display_cleanup(void);

// helper for X11 pending check
int x11_has_pending_events(void);

#endif