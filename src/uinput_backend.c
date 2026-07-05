#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include "uinput_backend.h"

static void emit_event(int fd, int type, int code, int value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    // gettimeofday(&ev.time, NULL) is omitted because setting time=0 tells the Linux kernel to apply its own high-precision monotonic timestamp.

    if (write(fd, &ev, sizeof(ev)) < 0)
    {
        perror("[uinput] :: [ERROR] :: failed writing input event");
    }
}

int uinput_init(void)
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        fprintf(stderr, "[uinput] :: [WARN] :: cannot open /dev/uinput (check udev permissions/input group).\n");
        return -1;
    }

    // enable Key events
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    // register only the specific keycodes we need
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFTCTRL);
    ioctl(fd, UI_SET_KEYBIT, KEY_V);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFTSHIFT);
    ioctl(fd, UI_SET_KEYBIT, KEY_INSERT);

    // configure virtual hardware identification
    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x7236;  // arbitrary
    usetup.id.product = 0x0909; // arbitrary
    strcpy(usetup.name, "clipd-virtual-keyboard");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    if (ioctl(fd, UI_DEV_CREATE) < 0)
    {
        perror("[uinput] :: [ERROR] :: UI_DEV_CREATE failed");
        close(fd);
        return -1;
    }

    printf("[uinput] :: [SUCCESS] :: virtual keyboard registered in linux kernel.\n");
    return fd;
}

void uinput_inject_ctrl_v(int fd)
{
    // simulates shift + insert for universal pasting

    if (fd < 0)
        return;

    printf("[uinput] :: [DEBUG] :: Injecting synthetic Ctrl+V sequence...\n");

    // Left Shift
    emit_event(fd, EV_KEY, KEY_LEFTSHIFT, 1);
    emit_event(fd, EV_SYN, SYN_REPORT, 0);

    usleep(15000);

    // Ins
    emit_event(fd, EV_KEY, KEY_INSERT, 1);
    emit_event(fd, EV_SYN, SYN_REPORT, 0);

    usleep(15000);

    emit_event(fd, EV_KEY, KEY_INSERT, 0);
    emit_event(fd, EV_SYN, SYN_REPORT, 0);

    usleep(15000);

    // release Left Shift
    emit_event(fd, EV_KEY, KEY_LEFTSHIFT, 0);
    emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

void uinput_cleanup(int fd)
{
    if (fd >= 0)
    {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
}