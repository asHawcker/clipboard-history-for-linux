#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include "ring_buffer.h"

static Display *dpy = NULL;
static Window dummy_win = 0;

static Atom clipboard_atom = 0;
static Atom utf8_atom = 0;
static Atom clipd_prop_atom = 0;

static int xfixes_event_base = 0;
static int xfixes_error_base = 0;

int x11_init(void)
{
    // connection to the XServer/XWayland socket
    dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        fprintf(stderr, "[display] :: [ERROR] :: cannot open X Display\n");
        return -1;
    }

    if (!XFixesQueryExtension(dpy, &xfixes_event_base, &xfixes_error_base))
    {
        fprintf(stderr, "[X11] :: [ERROR] :: XFixes extension not supported by X Server!\n");
        return -1;
    }

    // CLIPBOARD string atom
    clipboard_atom = XInternAtom(dpy, "CLIPBOARD", False);
    utf8_atom = XInternAtom(dpy, "UTF8_STRING", False);
    clipd_prop_atom = XInternAtom(dpy, "CLIPD_SEL_PROP", False);

    // window to attach the event listener
    Window root = DefaultRootWindow(dpy);
    dummy_win = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);

    // monitor CLIPBOARD updates with XFixes
    XFixesSelectSelectionInput(dpy, dummy_win, clipboard_atom, XFixesSetSelectionOwnerNotifyMask);
    XFlush(dpy);

    printf("[display] :: [SUCCESS] :: monitoring clipboard via XWayland Bridge\n");

    // connection file descriptor
    return ConnectionNumber(dpy);
}

void x11_handle_event(ring_buffer_t *rb)
{
    XEvent event;

    while (XPending(dpy))
    {
        XNextEvent(dpy, &event);

        // check if the event is an XFixes selection change notification
        if (event.type == xfixes_event_base + XFixesSetSelectionOwnerNotify)
        {
            XFixesSelectionNotifyEvent *ev = (XFixesSelectionNotifyEvent *)&event;

            if (ev->selection == clipboard_atom && ev->owner != dummy_win)
            {
                printf("[display] :: [DEBUG] :: Owner changed! Requesting UTF8_STRING conversion\n");

                // request the X server to put the UTF8 data into our clipd_prop_atom property
                XConvertSelection(dpy, clipboard_atom, utf8_atom, clipd_prop_atom, dummy_win, CurrentTime);
            }
        }
        else if (event.type == SelectionNotify)
        {
            XSelectionEvent *sel = &event.xselection;

            // successfully validated the data
            if (sel->property == clipd_prop_atom && sel->target == utf8_atom)
            {
                Atom actual_type;
                int actual_format;
                unsigned long nitems;
                unsigned long bytes_after;
                unsigned char *prop_data = NULL;

                // read the property data from the X Server
                // we request upto MAX_PAYLOAD_SIZE
                if (XGetWindowProperty(dpy, dummy_win, clipd_prop_atom, 0, MAX_PAYLOAD_SIZE / 4, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success)
                {

                    if (actual_type == utf8_atom && prop_data != NULL && nitems > 0)
                    {
                        // push raw text bytes to our ring buffer
                        uint32_t id = rb_push(rb, (const char *)prop_data, nitems);
                        printf("[display] :: [SUCCESS] :: Saved %lu bytes of live text as ID %u.\n", nitems, id);
                    }

                    // free Xlib allocated memory to prevent leaks
                    if (prop_data)
                        XFree(prop_data);
                }

                // delete the property from our dummy window to clean up X Server
                XDeleteProperty(dpy, dummy_win, clipd_prop_atom);
            }
        }
    }
}

void x11_cleanup(void)
{
    if (dpy)
    {
        if (dummy_win)
            XDestroyWindow(dpy, dummy_win);
        XCloseDisplay(dpy);
        dpy = NULL;
    }
}

Display *x11_get_display(void)
{
    return dpy;
}