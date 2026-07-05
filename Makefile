CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -Iinclude -Isrc -g
LDFLAGS = -lwayland-client -lX11 -lXfixes

PROTO_XML = protocols/wlr-data-control-unstable-v1.xml
PROTO_HDR = src/wlr-data-control-unstable-v1-client-protocol.h
PROTO_SRC = src/wlr-data-control-unstable-v1-protocol.c
PROTO_OBJ = $(PROTO_SRC:.c=.o)

SRCS_DAEMON = src/clipd.c src/ipc_server.c src/ring_buffer.c src/display.c src/wayland_backend.c src/x11_backend.c src/uinput_backend.c
SRCS_CLIENT = src/clipboard.c

OBJS_DAEMON = $(SRCS_DAEMON:.c=.o) $(PROTO_OBJ)
OBJS_CLIENT = $(SRCS_CLIENT:.c=.o)

all: clipd clipboard

# generate the wayland protocol's C code from xml
$(PROTO_SRC): $(PROTO_XML)
	wayland-scanner private-code $< $@

# generate the wayland protocol header from xml
$(PROTO_HDR): $(PROTO_XML)
	wayland-scanner client-header $< $@

$(SRCS_DAEMON:.c=.o): $(PROTO_HDR)

clipd: $(OBJS_DAEMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clipboard: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o clipd clipboard $(PROTO_HDR) $(PROTO_SRC)	

.PHONY: all clean