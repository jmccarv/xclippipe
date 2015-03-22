#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <inttypes.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

typedef enum xcp_atom_idx_t {
    WM_PROTOCOLS = 0,
    WM_DELETE_WINDOW = 1,
    CLIPBOARD = 2,
    _XCP_CLIP = 3
} xcp_atom_idx_t;

xcb_atom_t xcp_atom[4];

Display *dpy;
xcb_connection_t *c;

static XrmOptionDescRec opTable[] = {
    { "-background",    "*background",  XrmoptionSepArg,    (XPointer) NULL  },
    { "-bg",            "*background",  XrmoptionSepArg,    (XPointer) NULL  },
    { "-display",       "*display",     XrmoptionSepArg,    (XPointer) NULL  },
    { "-foreground",    "*foreground",  XrmoptionSepArg,    (XPointer) NULL  },
    { "-fg",            "*foreground",  XrmoptionSepArg,    (XPointer) NULL  },
    { "-geometry",      "*geometry",    XrmoptionSepArg,    (XPointer) NULL  },
    { "-g",             "*geometry",    XrmoptionSepArg,    (XPointer) NULL  },
    { "-name",          "*name",        XrmoptionSepArg,    (XPointer) NULL  },
    { "-sticky",        "*sticky",      XrmoptionSepArg,    (XPointer) "on"  },
    { "+sticky",        "*sticky",      XrmoptionSepArg,    (XPointer) "off" },
    { "-title",         "*title",       XrmoptionSepArg,    (XPointer) NULL  },
    { "-help",          NULL,           XrmoptionSkipArg,   (XPointer) NULL  },
    { "-version",       NULL,           XrmoptionSkipArg,   (XPointer) NULL  },
    { "-?",             NULL,           XrmoptionSkipArg,   (XPointer) NULL  },
};

xcb_screen_t *get_screen (int screen_num) {
    int i;

    /* Get the screen with number screen_num */
    const xcb_setup_t *setup = xcb_get_setup(c);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    
    for (i=0; i < screen_num; i++) {
        xcb_screen_next(&iter);
    }

    return (iter.data);
}

void redraw (xcb_window_t window, xcb_gcontext_t foreground) {
    /*
    xcb_rectangle_t rect;
    xcb_poly_fill_rectangle(window->root, foreground, 
    */
}

void ev_selection_notify (xcb_window_t window, xcb_selection_notify_event_t *event) {
    xcb_get_property_cookie_t prop_cookie;
    xcb_get_property_reply_t  *prop;
    //char *str;

    prop_cookie = xcb_get_property(c, False, window, event->property, XCB_ATOM_STRING, 0, UINT32_MAX);
    prop        = xcb_get_property_reply(c, prop_cookie, NULL);
    
    if (!prop) {
        fprintf(stderr, "No propery reply\n");
        return;
    }

    
    printf("Got property reply\n");
    printf("resp type=%d format=%d seq=%d length=%d type=%d bytes_after=%u value_len=%u\n", 
           prop->type, prop->format, prop->sequence, prop->length, prop->type, prop->bytes_after, prop->value_len);

    if (prop->value_len < 1) {
        fprintf(stderr, "Property reply was zero length\n");
        free(prop);
        return;
    }
    

    if (prop->type == XCB_ATOM_STRING && prop->format == 8) {
        /*
        if (NULL == (str = malloc(prop->value_len+1))) {
            fprintf(stderr, "Failed to malloc() memory\n");
            free(prop);
            return;
        }

        memset(str, '\0', prop->value_len);
        memcpy(str, (char *)xcb_get_property_value(prop), prop->value_len);

        printf("Got String: %s\n", str);
        */
        printf("Got String: %*s\n", prop->value_len,(char *)xcb_get_property_value(prop));

    } else {
        fprintf(stderr, "Got something that's not a string; ignoring\n");
    }

    if (prop->bytes_after) {
        fprintf(stderr, "Did not receive all the data from the clipboard, output truncated\n");
    }

    free(prop);

    xcb_delete_property(c, window, event->property);
}


void ev_button_press (xcb_window_t window, xcb_button_press_event_t *event) {
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;

    if (event->detail != 2) return;

    cookie = xcb_get_selection_owner(c, XCB_ATOM_PRIMARY);
    reply  = xcb_get_selection_owner_reply(c, cookie, NULL);

    if (reply->owner == None) {
        fprintf(stderr, "No selection owner\n");
        return;
    }

    xcb_convert_selection(c, window, XA_PRIMARY, XA_STRING, xcp_atom[_XCP_CLIP], CurrentTime);
    xcb_flush(c);
    free(reply);
}

void send_close_message (xcb_window_t window) {
    xcb_client_message_event_t *event = calloc(32, 1);
    
    event->response_type  = XCB_CLIENT_MESSAGE;
    event->format         = 32;
    event->window         = window;
    event->type           = xcp_atom[WM_PROTOCOLS];
    event->data.data32[0] = xcp_atom[WM_DELETE_WINDOW];

    xcb_send_event(c, False, window, XCB_EVENT_MASK_NO_EVENT, (char *)event);
    xcb_flush(c);

    free(event);
}

void ev_key_press (xcb_window_t window, xcb_key_press_event_t *event) {
    KeySym ks = XkbKeycodeToKeysym(dpy, event->detail, 0, 0);
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;

    char *ch = XKeysymToString(ks);

    if (strlen(ch) != 1) return;
    if (! (event->state & XCB_KEY_BUT_MASK_CONTROL)) return;

    switch (*ch) {
        case 'd':
            send_close_message(window);
            break;

        case 'v':
            cookie = xcb_get_selection_owner(c, xcp_atom[CLIPBOARD]);
            reply  = xcb_get_selection_owner_reply(c, cookie, NULL);
            
            if (reply->owner != None) {
                printf("owner=%d\n", reply->owner);
                xcb_convert_selection(c, window, xcp_atom[CLIPBOARD], XA_STRING, xcp_atom[_XCP_CLIP], CurrentTime);
                xcb_flush(c);
            }

            free(reply);
            break;
    }
}

xcb_atom_t intern_atom (const char *atom) {
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;

    if (!atom || !strlen(atom))
        return XCB_ATOM_NONE;

    cookie = xcb_intern_atom(c, 0, strlen(atom), atom);
    reply  = xcb_intern_atom_reply(c, cookie, NULL);
    printf("intern %s => %d\n", atom, reply->atom);
    return reply->atom;
}

void intern_atoms () {
    xcp_atom[WM_PROTOCOLS]     = intern_atom("WM_PROTOCOLS");
    xcp_atom[WM_DELETE_WINDOW] = intern_atom("WM_DELETE_WINDOW");
    xcp_atom[CLIPBOARD]        = intern_atom("CLIPBOARD");
    xcp_atom[_XCP_CLIP]        = intern_atom("_XCP_CLIP");
}

void xclippipe () {
    int done = 0;

    intern_atoms();

    xcb_screen_t *screen = get_screen(DefaultScreen(dpy));

    /* create black (foreground) graphic cnotext */
    xcb_drawable_t window = screen->root;
    xcb_gcontext_t foreground = xcb_generate_id(c);
    uint32_t       mask      = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    uint32_t       values[2] = { screen->white_pixel, 0 };
    xcb_create_gc(c, foreground, window, mask, values);

    /* create window */
    window = xcb_generate_id(c);

    //printf("black=%d  white=%d\n", screen->black_pixel, screen->white_pixel);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = screen->black_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_PRESS;

    xcb_create_window(c, XCB_COPY_FROM_PARENT, window, screen->root,
                      0, 0, 150, 150, 10,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, values);

    /* set window title */
    char *title = "xclippipe";
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(title), title);

    /* Set up to handle clicking the close button */
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, xcp_atom[WM_PROTOCOLS], 4, 32, 1, &xcp_atom[WM_DELETE_WINDOW]);

    /* show window */
    xcb_map_window(c, window);
    xcb_flush(c);

    xcb_generic_event_t *event;
    while (!done && (event = xcb_wait_for_event(c))) {
        printf("Got Event\n");
        switch (event->response_type & ~0x80) {
            case XCB_EXPOSE:
                redraw(window, foreground);
                break;

            case XCB_SELECTION_NOTIFY:
                ev_selection_notify(window, (xcb_selection_notify_event_t *)event);
                break;

            case XCB_CLIENT_MESSAGE:
                if ((*(xcb_client_message_event_t *)event).data.data32[0] == xcp_atom[WM_DELETE_WINDOW]) {
                    done = 1;
                }
                break;

            case XCB_BUTTON_PRESS:
                ev_button_press(window, (xcb_button_press_event_t *)event);
                break;

            case XCB_KEY_PRESS:
                ev_key_press(window, (xcb_key_press_event_t *)event);
                break;
        }
        
        free(event);
    }

    return;
}

int main (int argc, char **argv) {
    
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Could not open display\n");
        exit(EXIT_FAILURE);
    }

    c = XGetXCBConnection(dpy);
    if (!c) {
        fprintf(stderr, "Could not cast Display object to an XCBConnection object\n");
        exit(EXIT_FAILURE);
    }

    //XrmInitialize();
    //XrmParseCommand(NULL, opTable, 

    xclippipe();

    xcb_disconnect(c);
    return 0;
}
