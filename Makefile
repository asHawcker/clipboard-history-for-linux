# =========================================================================
# Clip History Engine - Production Distribution Makefile
# =========================================================================

# Compiler & Flags
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -pedantic -std=c11 -Iinclude -Isrc -O2 -g
LDFLAGS ?= -lwayland-client -lX11 -lXfixes

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

# Binaries & Scripts
DAEMON  = clipd
CLIENT  = clipboard
SCRIPT  = cb-popup.sh

# Wayland Protocols
PROTO_XML = protocols/wlr-data-control-unstable-v1.xml
PROTO_HDR = $(SRC_DIR)/wlr-data-control-unstable-v1-client-protocol.h
PROTO_SRC = $(SRC_DIR)/wlr-data-control-unstable-v1-protocol.c
PROTO_OBJ = $(OBJ_DIR)/wlr-data-control-unstable-v1-protocol.o

# Source Files
SRCS_DAEMON = $(SRC_DIR)/clipd.c $(SRC_DIR)/ipc_server.c $(SRC_DIR)/ring_buffer.c \
              $(SRC_DIR)/display.c $(SRC_DIR)/wayland_backend.c $(SRC_DIR)/x11_backend.c \
              $(SRC_DIR)/uinput_backend.c
SRCS_CLIENT = $(SRC_DIR)/clipboard.c

# Object Files (Mapped to obj/ directory)
OBJS_DAEMON = $(SRCS_DAEMON:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(PROTO_OBJ)
OBJS_CLIENT = $(SRCS_CLIENT:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Standard Installation Paths
PREFIX           ?= /usr/local
BINDIR           ?= $(PREFIX)/bin
REAL_HOME        ?= $(if $(SUDO_USER),/home/$(SUDO_USER),$(HOME))
SYSTEMD_USER_DIR ?= $(REAL_HOME)/.config/systemd/user

# Debian Packaging Settings
DEB_PKG_DIR      = deb-package
DEB_NAME         = clip-history_1.0.0_amd64.deb

.PHONY: all clean install uninstall deb

all: $(DAEMON) $(CLIENT)

# =========================================================================
# Wayland Protocol Generation
# =========================================================================
$(PROTO_SRC): $(PROTO_XML)
	wayland-scanner private-code $< $@

$(PROTO_HDR): $(PROTO_XML)
	wayland-scanner client-header $< $@

# Ensure headers exist before compiling any daemon source files
$(OBJS_DAEMON): | $(PROTO_HDR)

# =========================================================================
# Build Rules
# =========================================================================
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Compile C files into Object files inside obj/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link Daemon Binary
$(DAEMON): $(OBJS_DAEMON)
	$(CC) $^ -o $@ $(LDFLAGS)

# Link Client Binary
$(CLIENT): $(OBJS_CLIENT)
	$(CC) $^ -o $@ $(LDFLAGS)

# =========================================================================
# Installation & Packaging Hooks
# =========================================================================
install: all
	@echo ":: Installing core binaries to $(DESTDIR)$(BINDIR)..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(DAEMON) $(DESTDIR)$(BINDIR)/$(DAEMON)
	install -m 0755 $(CLIENT) $(DESTDIR)$(BINDIR)/$(CLIENT)

	@echo ":: Installing GUI popup script as $(DESTDIR)$(BINDIR)/cb-popup..."
	if [ -f $(SCRIPT) ]; then \
		install -m 0755 $(SCRIPT) $(DESTDIR)$(BINDIR)/cb-popup; \
	fi

	@echo ":: Installing systemd user service..."
	install -d $(DESTDIR)$(SYSTEMD_USER_DIR)
	if [ -f clipd.service ]; then \
		install -m 0644 clipd.service $(DESTDIR)$(SYSTEMD_USER_DIR)/clipd.service; \
	fi

	@echo ""
	@echo "-------------------------------------------------------------------"
	@echo " SUCCESS: Installation complete!"
	@echo "-------------------------------------------------------------------"
	@echo " 1. Reload systemd units: systemctl --user daemon-reload"
	@echo " 2. Enable & start service: systemctl --user enable --now clipd.service"
	@echo " 3. Bind your shortcut to: cb-popup"
	@echo "-------------------------------------------------------------------"

uninstall:
	@echo ":: Stopping systemd service (if active)..."
	-systemctl --user stop clipd.service 2>/dev/null || true
	-systemctl --user disable clipd.service 2>/dev/null || true

	@echo ":: Removing binaries from $(DESTDIR)$(BINDIR)..."
	rm -f $(DESTDIR)$(BINDIR)/$(DAEMON)
	rm -f $(DESTDIR)$(BINDIR)/$(CLIENT)
	rm -f $(DESTDIR)$(BINDIR)/cb-popup

	@echo ":: Removing systemd user service..."
	rm -f $(DESTDIR)$(SYSTEMD_USER_DIR)/clipd.service
	-systemctl --user daemon-reload 2>/dev/null || true

	@echo ":: Clean uninstallation complete."

# =========================================================================
# Debian Packaging (.deb)
# =========================================================================
deb: all
	@echo ":: Assembling files for Debian packaging..."
	mkdir -p $(DEB_PKG_DIR)/usr/bin
	mkdir -p $(DEB_PKG_DIR)/usr/lib/systemd/user

	install -m 0755 $(DAEMON) $(DEB_PKG_DIR)/usr/bin/$(DAEMON)
	install -m 0755 $(CLIENT) $(DEB_PKG_DIR)/usr/bin/$(CLIENT)
	if [ -f $(SCRIPT) ]; then \
		install -m 0755 $(SCRIPT) $(DEB_PKG_DIR)/usr/bin/cb-popup; \
	fi

	if [ -f clipd.service ]; then \
		install -m 0644 clipd.service $(DEB_PKG_DIR)/usr/lib/systemd/user/clipd.service; \
	fi

	@echo ":: Building final .deb archive..."
	dpkg-deb --build $(DEB_PKG_DIR) $(DEB_NAME)
	@echo ":: Successfully generated $(DEB_NAME)!"

clean:
	rm -rf $(OBJ_DIR) $(DAEMON) $(CLIENT) $(PROTO_HDR) $(PROTO_SRC) $(DEB_NAME)