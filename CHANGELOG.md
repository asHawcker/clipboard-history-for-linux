# Changelog

All notable changes to the Clip History Engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-09-27

### Fixed
- **X11 Timestamp Rejection:** Fixed an issue where the daemon failed to paste historical items. The X server rejected `XSetSelectionOwner` requests using `CurrentTime` (0). The daemon now forces a `PropertyNotify` event to extract a valid, present-moment X Server timestamp to guarantee ownership overrides.
- **VSCode & Terminal Pasting:** Fixed a bug where `Shift + Insert` pasted the wrong text (or emitted an error beep in Ubuntu 24) in Terminals and VSCode. The daemon now claims and serves both the `CLIPBOARD` and `PRIMARY` selections.
- **Systemd Path Mismatch:** Fixed `clipd.service` to correctly point to `/usr/local/bin/clipd` so it starts properly when built and installed from source.

### Changed
- **Compositor Delay:** Increased the `usleep` window focus delay in `ipc_server.c` from 50ms to 200ms to allow time for window focus change before pasting.