# Clip History Engine (Linux / X11)

A lightweight, clipboard history daemon (`clipd`) and command-line engine (`clipboard`) written from scratch in pure C.

This engine monitors desktop clipboard selections in real time, buffers history in memory using a custom ring buffer, communicates over authenticated UNIX domain sockets, and performs universal text pasting via Linux kernel virtual hardware injection (`/dev/uinput`) .

---

## Tested Environment

This software has been compiled, deployed, and strictly verified on:

- **Operating System:** Ubuntu 22.04 LTS (Jammy Jellyfish) `amd64` / `x86_64`
- **Display Server:** X11 (`SESSION_X11`)
- **Window Managers:** GNOME Desktop

---

## Architecture & Key Features

### 1. In-Memory Ring Buffer (`src/ring_buffer.c`)

- **Bounded Memory Footprint:** Implements a circular queue storing up to `50` recent clipboard entries with a maximum payload cap of `10 MB` per item.
- **Zero-Leak Rotation:** When the buffer reaches capacity, the oldest entry is dynamically freed and replaced.

### 2. Hardened IPC Socket Server (`src/ipc_server.c`)

- **UNIX Domain Sockets:** Daemon hosts an isolated socket at `$XDG_RUNTIME_DIR/clipd.sock`
- **Strict Permissions (`umask 0077`):** The listening socket is bound with `0600` file permissions so only the logged-in user can access it.
- **Kernel Credential Verification:** Uses `getsockopt(..., SO_PEERCRED, ...)` to inspect the peer process UID at the kernel level. Connection attempts from non-matching user accounts are dropped immediately.
- **Binary Protocol:** Uses a packed C struct (`ipc_header_t`) prefixed with a unique identifier to prevent unauthorized packet processing.

### 3. Native X11 & XFixes Selection Monitoring (`src/x11_backend.c`)

- Leverages the `XFixes` extension (`XFixesSelectSelectionInput`) on a hidden dummy window to asynchronously receive `XFixesSetSelectionOwnerNotify` events whenever any desktop application copies text.
- Automatically converts and extracts `UTF8_STRING` selection payloads and pushes them into the daemon's ring buffer.
- Claims X11 clipboard ownership on demand when serving clips back to target applications.

### 4. Virtual Hardware Keystroke Injection (`src/uinput_backend.c`)

- **`/dev/uinput` Kernel Integration:** Registers a virtual USB keyboard (`clipd-virtual-keyboard`) directly inside the Linux kernel input subsystem.
- **Universal Pasting (`Shift + Insert`):** When triggering a paste command via CLI or UI popup, the daemon sets the X11 clipboard ownership to the selected clip, sleeps for window focus transition, and emits synthetic `EV_KEY` events to simulate a universal paste sequence.

### 5. Fast GUI Popup Launcher

- Uses a shell wrapper that fetches history over IPC and pops up an interactive window to select the text.

---

## System Prerequisites

Install the required X11 development headers, build utilities, and menu dependencies:

```bash
sudo apt update
sudo apt install build-essential gcc make libx11-dev libxfixes-dev rofi
```

## Building & Installation

You can install the engine either by building directly from source or by assembling a native Debian `(.deb)` package.

### Option A: Build & Install from Source

```bash
# 1. Compile clean objects and binaries
make clean && make

# 2. Install binaries to /usr/local/bin and service files
sudo make install
```

### Option B: Build & Install via Debian Package `(.deb)`

```bash
# 1. Build the native Debian package (.deb)
make clean && make deb

# 2. Install the generated package via APT
sudo apt install ./clip-history_1.0.0_amd64.deb
```

## Post-Installation Setup

1. Enable the Systemd User Daemon
   The clipboard daemon runs natively as a user-scoped service tied to your graphical session:

   ```bash
   systemctl --user daemon-reload
   systemctl --user enable --now clipd.service
   ```

2. Configure /dev/uinput Permissions
   Because injecting synthetic keystrokes requires write access to /dev/uinput, ensure your user account can write to the virtual device without root privileges . Create a permanent udev rule:

   ```bash
   sudo nano /etc/udev/rules.d/99-uinput.rules
   ```

   Add the following line:

   ```ini
   KERNEL=="uinput", SUBSYSTEM=="misc", OPTIONS+="static_node=uinput", TAG+="uaccess", GROUP="input", MODE="0660"
   ```

   Reload rules and trigger:

   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger --sysname-match=uinput
   ```

3. Bind the Graphical Popup Shortcut (Super + V)
   To trigger the interactive Rofi menu from anywhere on your desktop, bind your preferred shortcut (e.g., Super + V or Ctrl + Shift + V) to the launcher script:
   - Command Path (Make install): /usr/local/bin/cb-popup

   - Command Path (Debian install): /usr/bin/cb-popup

   ### Via GNOME Terminal (gsettings):

   ```bash
   KEY_PATH="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/cliphistory/"
   gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "['$KEY_PATH']"
   gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEY_PATH name 'Clipboard History'
   gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEY_PATH command '/usr/bin/cb-popup'
   gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEY_PATH binding '<Super>v'
   ```

## Command-Line Interface (CLI)

You can interact with the background daemon directly from the terminal using the clipboard binary :

```bash
# Add manual text payload to history
clipboard add "Custom string entry"

# List all buffered clips with their numerical IDs
clipboard list

# Get raw payload output for a specific clip ID
clipboard get 4

# Trigger immediate X11 selection ownership and inject synthetic paste for ID
clipboard paste 4
```

## Uninstallation

If installed via Source (`make install`):

```bash
sudo make uninstall
```

If installed via Debian Package (`.deb`):

```bash
# Stop active user daemon first
systemctl --user stop clipd.service 2>/dev/null || true
systemctl --user disable clipd.service 2>/dev/null || true

# Remove package
sudo apt remove clip-history
```

## Project Structure

```
clip-history/
├── include/
│   ├── display.h        # Display abstraction & X11 structures
│   ├── ipc_server.h     # Socket server definitions
│   ├── protocol.h       # Binary IPC packet structures & error codes
│   ├── ring_buffer.h    # Circular buffer memory layout
│   └── uinput_backend.h # Virtual keyboard definitions
├── src/
│   ├── clipd.c          # Main daemon event loop (poll system call)
│   ├── clipboard.c      # CLI client binary implementation
│   ├── display.c        # Session detection router
│   ├── ipc_server.c     # Secure UNIX socket authentication & routing
│   ├── ring_buffer.c    # Circular memory buffer operations
│   ├── uinput_backend.c # /dev/uinput virtual hardware keystroke injection
│   └── x11_backend.c    # XLib & XFixes clipboard monitoring & ownership
├── cb-popup.sh          # Rofi GUI integration script
├── clipd.service        # Systemd user service unit file
└── Makefile             # Production distribution build & .deb packager
```
