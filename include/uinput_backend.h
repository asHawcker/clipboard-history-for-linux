#ifndef UINPUT_BACKEND_H
#define UINPUT_BACKEND_H

// initializes the /dev/uinput virtual keyboard
// returns the file descriptor
int uinput_init(void);

// injects the Ctrl + V keystroke combination safely.
void uinput_inject_ctrl_v(int fd);

// destroys the virtual device and closes the file descriptor.
void uinput_cleanup(int fd);

#endif