#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <wayland-client.h>
#include "wlr-data-control-unstable-v1-client-protocol.h"
#include "ring_buffer.h"

static struct wl_display *display = NULL;
static struct wl_registry *registry = NULL;
static struct wl_seat *seat = NULL;
static struct zwlr_data_control_manager_v1 *data_control_manager = NULL;
static struct zwlr_data_control_device_v1 *data_device = NULL;

// data Source for CLIPBOARD
static struct zwlr_data_control_source_v1 *current_source = NULL;
static char *current_source_data = NULL;
static size_t current_source_size = 0;

// data Source for PRIMARY
static struct zwlr_data_control_source_v1 *current_primary_source = NULL;
static char *current_primary_source_data = NULL;
static size_t current_primary_source_size = 0;

static int is_our_source_active = 0;

// offer tracking
typedef struct {
    struct zwlr_data_control_offer_v1 *offer;
    int offers_utf8;
    int offers_plain;
} wayland_offer_t;

static wayland_offer_t *pending_offer = NULL;

// struct for asynchronous Pipe Transfer tracking
typedef struct {
    int read_fd;
    char *buffer;
    size_t capacity;
    size_t bytes_read;
} wayland_transfer_t;

static wayland_transfer_t curr_transfer = {
    .read_fd = -1,
    .buffer = NULL,
    .capacity = 0,
    .bytes_read = 0
};

static void reset_current_transfer(void)
{
    if (curr_transfer.read_fd != -1)
    {
        close(curr_transfer.read_fd);
        curr_transfer.read_fd = -1;
    }
    if (curr_transfer.buffer)
    {
        free(curr_transfer.buffer);
        curr_transfer.buffer = NULL;
    }
    curr_transfer.capacity = 0;
    curr_transfer.bytes_read = 0;
}

// +++++ Offer Listeners ++++++
static void offer_handle_offer(void *data, struct zwlr_data_control_offer_v1 *offer, const char *mime_type)
{
    wayland_offer_t *wo = (wayland_offer_t *)data;
    if (!wo || wo->offer != offer) return;

    if (strcmp(mime_type, "text/plain;charset=utf-8") == 0)
    {
        wo->offers_utf8 = 1;
    }
    else if (strcmp(mime_type, "text/plain") == 0)
    {
        wo->offers_plain = 1;
    }
}

static const struct zwlr_data_control_offer_v1_listener offer_listener = {
    .offer = offer_handle_offer,
};

// -===== device Listeners =====
static void device_handle_data_offer(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
    (void)data;
    (void)device;

    if (pending_offer)
    {
        if (pending_offer->offer)
        {
            zwlr_data_control_offer_v1_destroy(pending_offer->offer);
        }
        free(pending_offer);
        pending_offer = NULL;
    }

    if (!offer) return;

    pending_offer = calloc(1, sizeof(wayland_offer_t));
    if (!pending_offer) return;

    pending_offer->offer = offer;
    zwlr_data_control_offer_v1_add_listener(offer, &offer_listener, pending_offer);
}

static void device_handle_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
    (void)data;
    (void)device;

    if (!offer) return;

    if (is_our_source_active)
    {
        printf("[wayland] :: [DEBUG] :: Ignoring selection event originating from our own data source.\n");
        return;
    }

    if (!pending_offer || pending_offer->offer != offer) return;

    const char *chosen_mime = NULL;
    if (pending_offer->offers_utf8)
        chosen_mime = "text/plain;charset=utf-8";
    else if (pending_offer->offers_plain)
        chosen_mime = "text/plain";

    if (!chosen_mime) return;

    reset_current_transfer();

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) return;

    int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    if (flags != -1)
        fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);

    zwlr_data_control_offer_v1_receive(offer, chosen_mime, pipe_fds[1]);
    close(pipe_fds[1]);

    curr_transfer.read_fd = pipe_fds[0];
    curr_transfer.capacity = 4096;
    curr_transfer.buffer = calloc(1, curr_transfer.capacity);
    curr_transfer.bytes_read = 0;
}

static void device_handle_finished(void *data, struct zwlr_data_control_device_v1 *device)
{
    (void)data;
    (void)device;
}

static void device_handle_primary_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
    (void)data;
    (void)device;
    (void)offer;
}

static const struct zwlr_data_control_device_v1_listener device_listener = {
    .data_offer = device_handle_data_offer,
    .selection = device_handle_selection,
    .finished = device_handle_finished,
    .primary_selection = device_handle_primary_selection,
};

// ==== shared data source listeners ---
typedef struct {
    char *data;
    size_t size;
} source_payload_t;

static void source_handle_send(void *data, struct zwlr_data_control_source_v1 *source, const char *mime_type, int32_t fd)
{
    (void)source;
    (void)mime_type;
    source_payload_t *payload = (source_payload_t *)data;

    if (payload && payload->data && payload->size > 0)
    {
        size_t total_written = 0;
        while (total_written < payload->size)
        {
            ssize_t ret = write(fd, payload->data + total_written, payload->size - total_written);
            if (ret < 0)
            {
                if (errno == EINTR) continue;
                break;
            }
            total_written += ret;
        }
        printf("[wayland] :: [INFO] :: Served %zu bytes of clipboard/primary data.\n", total_written);
    }
    close(fd);
}

static void source_handle_cancelled(void *data, struct zwlr_data_control_source_v1 *source)
{
    (void)data;
    is_our_source_active = 0;

    if (source)
        zwlr_data_control_source_v1_destroy(source);

    if (source == current_source)
    {
        current_source = NULL;
        free(current_source_data);
        current_source_data = NULL;
        current_source_size = 0;
    }
    if (source == current_primary_source)
    {
        current_primary_source = NULL;
        free(current_primary_source_data);
        current_primary_source_data = NULL;
        current_primary_source_size = 0;
    }
}

static const struct zwlr_data_control_source_v1_listener source_listener = {
    .send = source_handle_send,
    .cancelled = source_handle_cancelled,
};

void wayland_set_clipboard(const char *data, size_t size)
{
    if (!data_control_manager || !data || size == 0) return;

    // 1. clean up existing clipboard source
    if (current_source)
    {
        zwlr_data_control_source_v1_destroy(current_source);
        current_source = NULL;
    }
    free(current_source_data);
    current_source_data = malloc(size);
    memcpy(current_source_data, data, size);
    current_source_size = size;

    static source_payload_t cb_payload;
    cb_payload.data = current_source_data;
    cb_payload.size = current_source_size;

    current_source = zwlr_data_control_manager_v1_create_data_source(data_control_manager);
    zwlr_data_control_source_v1_add_listener(current_source, &source_listener, &cb_payload);
    zwlr_data_control_source_v1_offer(current_source, "text/plain;charset=utf-8");
    zwlr_data_control_source_v1_offer(current_source, "text/plain");

    // 2. clean up existing primary source
    if (current_primary_source)
    {
        zwlr_data_control_source_v1_destroy(current_primary_source);
        current_primary_source = NULL;
    }
    free(current_primary_source_data);
    current_primary_source_data = malloc(size);
    memcpy(current_primary_source_data, data, size);
    current_primary_source_size = size;

    static source_payload_t pri_payload;
    pri_payload.data = current_primary_source_data;
    pri_payload.size = current_primary_source_size;

    current_primary_source = zwlr_data_control_manager_v1_create_data_source(data_control_manager);
    zwlr_data_control_source_v1_add_listener(current_primary_source, &source_listener, &pri_payload);
    zwlr_data_control_source_v1_offer(current_primary_source, "text/plain;charset=utf-8");
    zwlr_data_control_source_v1_offer(current_primary_source, "text/plain");

    is_our_source_active = 1;

    // set both selections on the data device
    zwlr_data_control_device_v1_set_selection(data_device, current_source);
    zwlr_data_control_device_v1_set_primary_selection(data_device, current_primary_source);
    wl_display_flush(display);

    printf("[wayland] :: [INFO] :: Claimed both CLIPBOARD and PRIMARY selections (%zu bytes).\n", size);
}

// ==== registry & lifecycle ---
static void registry_handle_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version)
{
    (void)data;
    (void)version;

    if (strcmp(interface, wl_seat_interface.name) == 0)
    {
        seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
    }
    else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0)
    {
        uint32_t bind_version = (version < 2) ? version : 2;
        data_control_manager = wl_registry_bind(reg, name, &zwlr_data_control_manager_v1_interface, bind_version);
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
    display = wl_display_connect(NULL);
    if (!display) return -1;

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!data_control_manager || !seat) return -1;

    data_device = zwlr_data_control_manager_v1_get_data_device(data_control_manager, seat);
    if (!data_device) return -1;

    zwlr_data_control_device_v1_add_listener(data_device, &device_listener, NULL);
    wl_display_roundtrip(display);

    return wl_display_get_fd(display);
}

int wayland_get_fd(void)
{
    return display ? wl_display_get_fd(display) : -1;
}

void wayland_pre_poll(void)
{
    if (!display) return;
    while (wl_display_prepare_read(display) != 0)
        wl_display_dispatch_pending(display);
    wl_display_flush(display);
}

void wayland_post_poll(struct pollfd *pfd, ring_buffer_t *rb)
{
    (void)rb;
    if (!display) return;

    if (pfd && (pfd->revents & POLLIN))
    {
        wl_display_read_events(display);
        wl_display_dispatch_pending(display);
    }
    else
    {
        wl_display_cancel_read(display);
    }
}

int wayland_get_extra_pollfds(struct pollfd *fds, int max_fds)
{
    if (max_fds > 0 && curr_transfer.read_fd != -1)
    {
        fds[0].fd = curr_transfer.read_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        return 1;
    }
    return 0;
}

void wayland_handle_extra_pollfds(struct pollfd *fds, int count, ring_buffer_t *rb)
{
    if (count <= 0 || curr_transfer.read_fd == -1) return;

    if (fds[0].revents & (POLLIN | POLLHUP | POLLERR))
    {
        char temp_chunk[1024];
        ssize_t n = read(curr_transfer.read_fd, temp_chunk, sizeof(temp_chunk));

        if (n > 0)
        {
            if (curr_transfer.bytes_read + n < MAX_PAYLOAD_SIZE)
            {
                if (curr_transfer.bytes_read + n + 1 > curr_transfer.capacity)
                {
                    size_t new_cap = curr_transfer.capacity * 2;
                    if (new_cap < curr_transfer.bytes_read + n + 1)
                        new_cap = curr_transfer.bytes_read + n + 1;
                    char *new_buf = realloc(curr_transfer.buffer, new_cap);
                    if (new_buf)
                    {
                        curr_transfer.buffer = new_buf;
                        curr_transfer.capacity = new_cap;
                    }
                }

                if (curr_transfer.bytes_read + n + 1 <= curr_transfer.capacity)
                {
                    memcpy(curr_transfer.buffer + curr_transfer.bytes_read, temp_chunk, n);
                    curr_transfer.bytes_read += n;
                    curr_transfer.buffer[curr_transfer.bytes_read] = '\0';
                }
            }
        }
        else if (n == 0)
        {
            if (curr_transfer.bytes_read > 0 && curr_transfer.buffer)
            {
                uint32_t id = rb_push(rb, curr_transfer.buffer, curr_transfer.bytes_read);
                printf("[wayland] :: [SUCCESS] :: Saved %zu bytes of live text as ID %u.\n", curr_transfer.bytes_read, id);
            }
            reset_current_transfer();
        }
        else
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                reset_current_transfer();
        }
    }
}

void wayland_cleanup(void)
{
    reset_current_transfer();

    if (pending_offer)
    {
        if (pending_offer->offer)
            zwlr_data_control_offer_v1_destroy(pending_offer->offer);
        free(pending_offer);
        pending_offer = NULL;
    }

    if (current_source)
    {
        zwlr_data_control_source_v1_destroy(current_source);
        current_source = NULL;
    }
    free(current_source_data);
    current_source_data = NULL;

    if (current_primary_source)
    {
        zwlr_data_control_source_v1_destroy(current_primary_source);
        current_primary_source = NULL;
    }
    free(current_primary_source_data);
    current_primary_source_data = NULL;

    is_our_source_active = 0;

    if (data_device)
    {
        zwlr_data_control_device_v1_destroy(data_device);
        data_device = NULL;
    }
    if (data_control_manager)
    {
        zwlr_data_control_manager_v1_destroy(data_control_manager);
        data_control_manager = NULL;
    }
    if (seat)
    {
        wl_seat_destroy(seat);
        seat = NULL;
    }
    if (registry)
    {
        wl_registry_destroy(registry);
        registry = NULL;
    }
    if (display)
    {
        wl_display_disconnect(display);
        display = NULL;
    }
}