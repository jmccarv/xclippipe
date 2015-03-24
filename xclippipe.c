#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
//#include <xcb/xcb_aux.h>
#include <inttypes.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

#include "motif_window_hints.h"

typedef enum xcp_atom_idx_t {
    WM_PROTOCOLS = 0,
    WM_DELETE_WINDOW = 1,
    CLIPBOARD = 2,
    _XCP_CLIP = 3,
    _MOTIF_WM_HINTS = 4
} xcp_atom_idx_t;

xcb_atom_t xcp_atom[4];

Display *dpy;
xcb_connection_t *c;
xcb_screen_t     *screen;
xcb_window_t     window;

static XrmOptionDescRec opTable[] = {
    { "-background",    ".background",  XrmoptionSepArg,    (XPointer) NULL  },
    { "-bg",            ".background",  XrmoptionSepArg,    (XPointer) NULL  },
    { "-borderless",    ".borderless",  XrmoptionNoArg,     (XPointer) "on"  },
    { "+borderless",    ".borderless",  XrmoptionNoArg,     (XPointer) "off" },
    { "-display",       ".display",     XrmoptionSepArg,    (XPointer) NULL  },
    { "-geometry",      ".geometry",    XrmoptionSepArg,    (XPointer) NULL  },
    { "-g",             ".geometry",    XrmoptionSepArg,    (XPointer) NULL  },
    { "-sticky",        ".sticky",      XrmoptionNoArg,     (XPointer) "on"  },
    { "+sticky",        ".sticky",      XrmoptionNoArg,     (XPointer) "off" },
    { "-title",         ".title",       XrmoptionSepArg,    (XPointer) NULL  },
    { "-help",          NULL,           XrmoptionSkipArg,   (XPointer) NULL  },
    { "-version",       NULL,           XrmoptionSkipArg,   (XPointer) NULL  },
    { "-?",             NULL,           XrmoptionSkipArg,   (XPointer) NULL  },
};

XrmDatabase default_opts = NULL;
XrmDatabase opts = NULL;

char *option_defaults[] = {
    NULL, "-g", "100x100", "-bg", "black", "+borderless", "-title", "xclippipe", 
};


char res_name_buf[1024];
char res_class_buf[1024];
const char *get_resource (const char *name, const char *class) {
    char res_name_buf[1024]  = "xclippipe.";
    char res_class_buf[1024] = "XClipPipe.";
    char *rec_type;
    XrmValue rec_val;

    strcat(res_name_buf, name);
    strcat(res_class_buf, class ? class : name);

    printf("gr(): Looking up: '%s'\n", res_name_buf);
    XrmGetResource(opts, res_name_buf, res_class_buf, &rec_type, &rec_val);
    printf("gr(): got: '%s'\n", rec_val.addr);

    if (!rec_val.addr)
        XrmGetResource(default_opts, res_name_buf, res_class_buf, &rec_type, &rec_val);
    
    printf("gr(): returning: '%s'\n", rec_val.addr);
        
    return(rec_val.addr);
}

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

void ev_selection_notify (xcb_selection_notify_event_t *event) {
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


void ev_button_press (xcb_button_press_event_t *event) {
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

void send_close_message () {
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

void ev_key_press (xcb_key_press_event_t *event) {
    KeySym ks = XkbKeycodeToKeysym(dpy, event->detail, 0, 0);
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;

    char *ch = XKeysymToString(ks);

    if (strlen(ch) != 1) return;
    if (! (event->state & XCB_KEY_BUT_MASK_CONTROL)) return;

    switch (*ch) {
        case 'd':
            send_close_message();
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
    xcp_atom[_MOTIF_WM_HINTS]  = intern_atom("_MOTIF_WM_HINTS");
}

XColor *get_background_color (XColor *bg) {
    XColor bg_exact;
    const char *color_name = get_resource("background",NULL);
    Status ret;

    if (! (color_name && strlen(color_name)))
        color_name = "black";

    if (0 == (ret = XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), color_name, bg, &bg_exact))) {
        fprintf(stderr, "Failed to get background color '%s'; defaulting to 'black'\n", color_name);
        color_name = "black";
        ret = XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), color_name, bg, &bg_exact);
    }

    if (0 == ret) {
        fprintf(stderr, "Failed to get background color '%s'\n", color_name);
        return NULL;
    }

    printf("gxbc(%d): color: %s  pixel=%lu  rgb: %hu %hu %hu\n",ret, color_name, bg->pixel, bg->red, bg->green, bg->blue);

    return bg;
}

#if 0
/* For whatever reason, ucb_aux_parse_color only seemed to work with '#rrggbb'
 * syntax.  It doesn't work for plain names like 'blue' or the 'rgb:rr/gg/bb'
 * syntax.  I'm probably just missing something but for now I'm just going to
 * use XAllocNamedColor instead.
 */
xcb_alloc_named_color_reply_t *get_background_color () {
    xcb_alloc_named_color_cookie_t anc;
    xcb_alloc_named_color_reply_t *ancr;
    const char *color_name = get_resource("background",NULL);

    if (!color_name && !strlen(color_name))
        color_name = "black";

    uint16_t red=0, green=0, blue=0;
    xcb_aux_parse_color(color_name, &red, &green, &blue);
    printf("parsed '%s': %"PRIu16" %"PRIu16" %"PRIu16"\n", color_name, red, green, blue);
    exit(1);

    anc = xcb_alloc_named_color(c, screen->default_colormap, strlen(color_name), color_name);
    xcb_flush(c);
    ancr = xcb_alloc_named_color_reply(c, anc, NULL);

    if (!ancr) {
        fprintf(stderr, "Failed to get background color '%s'; defaulting to 'black'\n", color_name);
        color_name = "black";
        anc = xcb_alloc_named_color(c, screen->default_colormap, strlen(color_name), color_name);
        xcb_flush(c);
        ancr = xcb_alloc_named_color_reply(c, anc, NULL);
    }

    if (!ancr) {
        fprintf(stderr, "Failed to get background color: %s\n", color_name);
        return NULL;
    }

    //printf("pixel: %"PRIu32"  rgb: %"PRIu16" %"PRIu16" %"PRIu16"\n", ancr->pixel, ancr->visual_red, ancr->visual_green, ancr->visual_blue);
    return ancr;
}
#endif

void set_window_state () {
    /* set window state */

    xcb_ewmh_connection_t ewmh;
    memset(&ewmh, '\0', sizeof(xcb_ewmh_connection_t));
    xcb_intern_atom_cookie_t *iac = xcb_ewmh_init_atoms(c, &ewmh);
    xcb_flush(c);

    xcb_ewmh_init_atoms_replies(&ewmh, iac, NULL);

    xcb_ewmh_set_wm_state(&ewmh, window, 1, &(ewmh._NET_WM_STATE_STICKY));
    xcb_flush(c);

    xcb_ewmh_connection_wipe(&ewmh);


    /* borderless? */
    if (0 == strcmp (get_resource("borderless",NULL), "on")){
        mwm_hints_t hints;
        hints.flags = MWM_HINTS_DECORATIONS;
        hints.decorations = MWM_DECOR_NONE;
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, xcp_atom[_MOTIF_WM_HINTS], xcp_atom[_MOTIF_WM_HINTS], 32, 5, &hints);
    }
}

typedef struct geom_t {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int flags;
} geom_t;

void xclippipe () {
    int done = 0;
    XColor bg;
    geom_t g = { 0, 0, 0, 0 };

    intern_atoms();

    screen = get_screen(DefaultScreen(dpy));

    // allocate background color for window
    if (NULL == get_background_color(&bg))
        return;

    // create window
    window = xcb_generate_id(c);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2];
    values[0] = bg.pixel;
    //values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_PRESS;
    values[1] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_PRESS;

    g.flags = XParseGeometry(get_resource("geometry","NULL"), &(g.x), &(g.y), &(g.width), &(g.height));

    xcb_create_window(c, XCB_COPY_FROM_PARENT, window, screen->root,
                      g.x, g.y, g.width, g.height, 10,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, values);

    /* set window title */
    const char *title = get_resource("title", NULL);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(title), title);

    set_window_state();
    

    /* set up to handle clicking the close button */
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, xcp_atom[WM_PROTOCOLS], 4, 32, 1, &xcp_atom[WM_DELETE_WINDOW]);

    /* show window */
    xcb_map_window(c, window);

    /* make sure the window gets placed where the user wanted */
    if (g.flags & ( XValue | YValue )) {
        values[0] = g.x;
        values[1] = g.y;
        xcb_configure_window(c, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    }

    xcb_flush(c);

    xcb_generic_event_t *event;
    while (!done && (event = xcb_wait_for_event(c))) {
        printf("Got Event\n");
        switch (event->response_type & ~0x80) {
#if 0            
            case XCB_EXPOSE:
                //redraw(window, foreground);
                break;
#endif

            case XCB_SELECTION_NOTIFY:
                ev_selection_notify((xcb_selection_notify_event_t *)event);
                break;

            case XCB_CLIENT_MESSAGE:
                if ((*(xcb_client_message_event_t *)event).data.data32[0] == xcp_atom[WM_DELETE_WINDOW]) {
                    done = 1;
                }
                break;

            case XCB_BUTTON_PRESS:
                ev_button_press((xcb_button_press_event_t *)event);
                break;

            case XCB_KEY_PRESS:
                ev_key_press((xcb_key_press_event_t *)event);
                break;
        }
        
        free(event);
    }

    XFreeColors(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &(bg.pixel), 1, 0);
    return;
}

void load_resources (int *argc, char **argv) {
    opts = XrmGetStringDatabase(XResourceManagerString(dpy));

    int c = sizeof(option_defaults)/sizeof(char *);
    XrmParseCommand(&default_opts, opTable, sizeof(opTable)/sizeof(XrmOptionDescRec), "xclippipe", &c, option_defaults);

    XrmParseCommand(&opts, opTable, sizeof(opTable)/sizeof(XrmOptionDescRec), "xclippipe", argc, argv);

    XrmSetDatabase(dpy, opts);
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

    XrmInitialize();

    // Get resources from server and command line and get default options
    load_resources(&argc, argv);

    xclippipe();

    xcb_disconnect(c);
    return 0;
}
