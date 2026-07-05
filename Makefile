CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -Iinclude -g
LDFLAGS = -lwayland-client

SRCS_DAEMON = src/clipd.c src/ipc_server.c src/ring_buffer.c src/display.c src/wayland_backend.c
SRCS_CLIENT = src/clipboard.c

OBJS_DAEMON = $(SRCS_DAEMON:.c=.o)
OBJS_CLIENT = $(SRCS_CLIENT:.c=.o)

all: clipd clipboard

clipd: $(OBJS_DAEMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clipboard: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o clipd clipboard

.PHONY: all clean