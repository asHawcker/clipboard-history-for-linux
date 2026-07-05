#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include "wlr-data-control-unstable-v1-client-protocol.h"

// pointer to display connection
static struct wl_display *display = NULL;

static struct wl_registry *registry = NULL;
static struct wl_seat *seat = NULL;
static struct zwlr_data_control_manager_v1 *data_control_manager = NULL;
static struct zwlr_data_control_device_v1 *data_device = NULL;

// wayland registry listeners
static void registry_handle_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version)
{
    (void)data;
    (void)version;

    if (strcmp(interface, wl_seat_interface.name) == 0)
    {
        seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
        printf("[display] :: [INFO] :: found and bound global wl_seat (ID %u)\n", name);
    }
    else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0)
    {
        data_control_manager = wl_registry_bind(reg, name, &zwlr_data_control_manager_v1_interface, 1);
        printf("[display] :: [INFO] :: found and bound global zwlr_data_control_manager_v1 (ID %u)\n", name);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
    (void)data;
    (void)reg;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

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

    // get registry object
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    // perform a synchronous roundtrip.
    // This will block momentarily to send the registry request and process compositor's response
    // and ensuring data_control_manager and seat are populated before we proceed.
    wl_display_roundtrip(display);

    if (!data_control_manager || !seat)
    {
        fprintf(stderr, "[display] :: [ERROR] :: compositor does not support wlr-data-control or wl_seat is missing\n");
        return -1;
    }

    printf("[display] :: [INFO] :: connected to wlr-data-control manager\n");

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
        if (data_device)
            zwlr_data_control_device_v1_destroy(data_device);
        if (data_control_manager)
            zwlr_data_control_manager_v1_destroy(data_control_manager);
        if (seat)
            wl_seat_destroy(seat);
        if (registry)
            wl_registry_destroy(registry);
        wl_display_disconnect(display);
        display = NULL;
    }
}