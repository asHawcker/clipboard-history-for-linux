# Changelog

All notable changes to the Clip History Engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-09-28

### Added

- **Native Wayland Clipboard Support:** Fully implemented native Wayland clipboard monitoring and ownership via the `zwlr-data-control-unstable-v1` protocol.
- **Asynchronous Pipe Transfers:** Added non-blocking Unix pipe streaming (`O_NONBLOCK`) integrated into the main `poll()` loop to handle external clipboard data transfers without blocking the daemon.
- **Simultaneous Selection Claiming:** Configured Wayland data sources to simultaneously claim both `CLIPBOARD` and `PRIMARY` selections on paste operations, ensuring compatibility with both paste shortcuts `Ctrl+Shift+V` and `Shift+Insert`.
- **Self-Paste Loop Prevention:** Implemented active source tracking (`is_our_source_active`) to prevent the daemon from re-capturing its own historical pastes into the ring buffer as new entries.

### Fixed

- **Wayland Double-Free Crash:** Resolved a segmentation fault caused by dangling pointers when handling Wayland primary selection events.
- **Pipe Buffer Memory Safety:** Added strict null-termination safeguards (`calloc` and dynamic buffer padding) during pipe reads to prevent buffer overflows and string-handling segfaults.
- **Backend State:** Unified session and backend routing into a single source of truth in `display.c`, preventing event loops from continuing to poll stale Wayland sockets after falling back to XWayland.

### Changed

- **Documentation:** Updated `README.md` system prerequisites to include Wayland development libraries (`libwayland-dev`, `wayland-protocols`) and added window manager configuration guides for Sway and Hyprland (`Super + V` shortcuts).

## [1.0.1] - 2026-09-27

### Fixed

- **X11 Timestamp Rejection:** Fixed an issue where the daemon failed to paste historical items. The X server rejected `XSetSelectionOwner` requests using `CurrentTime` (0). The daemon now forces a `PropertyNotify` event to extract a valid, present-moment X Server timestamp to guarantee ownership overrides.
- **VSCode & Terminal Pasting:** Fixed a bug where `Shift + Insert` pasted the wrong text (or emitted an error beep in Ubuntu 24) in Terminals and VSCode. The daemon now claims and serves both the `CLIPBOARD` and `PRIMARY` selections.
- **Systemd Path Mismatch:** Fixed `clipd.service` to correctly point to `/usr/local/bin/clipd` so it starts properly when built and installed from source.

### Changed

- **Compositor Delay:** Increased the `usleep` window focus delay in `ipc_server.c` from 50ms to 200ms to allow time for window focus change before pasting.
- **Pop-up Window:** Changed Rofi popup to a normal window to better support focus
