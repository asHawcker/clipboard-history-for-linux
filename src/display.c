#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display.h"

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

    // fallbacks if XDG_SESSION_TYPE isn't set
    if (getenv("WAYLAND_DISPLAY"))
        return SESSION_WAYLAND;
    if (getenv("DISPLAY"))
        return SESSION_X11;

    return SESSION_UNKNOWN;
}

extern int wayland_init(void);

int init_display_listener(session_type_t session)
{
    if (session == SESSION_WAYLAND)
    {
        printf("[display] :: [INFO] :: initializing wayland backend...\n");
        return wayland_init();
    }
    else if (session == SESSION_X11)
    {
        fprintf(stderr, "[display] :: [ERROR] :: X11 backend\n");
        return -1;
    }

    fprintf(stderr, "[display] :: [ERROR] :: unknown display session.\n");
    return -1;
}