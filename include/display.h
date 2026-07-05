#ifndef DISPLAY_H
#define DISPLAY_H

#include "ring_buffer.h"

typedef enum
{
    SESSION_UNKNOWN = 0,
    SESSION_X11 = 1,
    SESSION_WAYLAND = 2
} session_type_t;

extern session_type_t current_active_session;
session_type_t detect_session_type(void);

// returns the file descriptor for the display server connection
int init_display_listener(session_type_t session);

int wayland_init(void);
int x11_init(void);
void x11_handle_event(ring_buffer_t *rb);

#endif