#include <stdio.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>

#include <xcp_motif_window_hints.h>
#include <xcp_global.h>
#include <xcp_xclippipe.h>
#include <xcp_options.h>

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

xcb_atom_t intern_atom (const char *atom) {
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;

    if (!atom || !strlen(atom))
        return XCB_ATOM_NONE;

    cookie = xcb_intern_atom(c, 0, strlen(atom), atom);
    reply  = xcb_intern_atom_reply(c, cookie, NULL);
    debug("intern %s => %d\n", atom, reply->atom);
    return reply->atom;
}

void intern_atoms () {
    xcp_atom[WM_PROTOCOLS]     = intern_atom("WM_PROTOCOLS");
    xcp_atom[WM_DELETE_WINDOW] = intern_atom("WM_DELETE_WINDOW");
    xcp_atom[CLIPBOARD]        = intern_atom("CLIPBOARD");
    xcp_atom[_XCP_CLIP]        = intern_atom("_XCP_CLIP");
    xcp_atom[_MOTIF_WM_HINTS]  = intern_atom("_MOTIF_WM_HINTS");
}

/* For whatever reason, ucb_aux_parse_color only seemed to work with '#rrggbb'
 * syntax.  It doesn't work for plain names like 'blue' or the 'rgb:rr/gg/bb'
 * syntax.  I'm probably just missing something but for now I'm just going to
 * use XAllocNamedColor instead.
 */
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

    debug("gxbc(%d): color: %s  pixel=%lu  rgb: %hu %hu %hu\n",ret, color_name, bg->pixel, bg->red, bg->green, bg->blue);

    return bg;
}

void set_window_state () {
    /* set window state */
    xcb_atom_t state[2];
    uint32_t   nr_state = 0;

    xcb_ewmh_connection_t ewmh;
    memset(&ewmh, '\0', sizeof(xcb_ewmh_connection_t));
    xcb_intern_atom_cookie_t *iac = xcb_ewmh_init_atoms(c, &ewmh);
    xcb_flush(c);

    xcb_ewmh_init_atoms_replies(&ewmh, iac, NULL);

    if (resource_true("sticky")) {
        state[nr_state++] = ewmh._NET_WM_STATE_STICKY;
    }


    if (resource_true("above")) {
        state[nr_state++] = ewmh._NET_WM_STATE_ABOVE;

    } else if (resource_true("below")) {
        state[nr_state++] = ewmh._NET_WM_STATE_BELOW;
    }

    xcb_ewmh_set_wm_state(&ewmh, window, nr_state, state);
    //xcb_flush(c);

    /* borderless? */
    if (resource_true("borderless")) {
        mwm_hints_t hints;
        hints.flags = MWM_HINTS_DECORATIONS;
        hints.decorations = MWM_DECOR_NONE;
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, xcp_atom[_MOTIF_WM_HINTS], xcp_atom[_MOTIF_WM_HINTS], 32, 5, &hints);
    }

    xcb_ewmh_connection_wipe(&ewmh);
}

typedef struct geom_t {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int flags;
} geom_t;

XColor win_bg;

void xcp_init (int *argc, char **argv) {
    XColor win_bg;
    geom_t g = { 0, 0, 0, 0 };

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

    screen = get_screen(DefaultScreen(dpy));

    // Get resources from server and command line and get default options
    load_resources(argc, argv);

    if (get_resource("_help", NULL))
        usage();

    if (get_resource("_fullhelp", NULL))
        full_help();

    opt.nl           = resource_true("newline") ? "\n" : "";
    opt.debug        = resource_true("_debug");
    opt.o_stdout     = resource_true("stdout");
    opt.flush_stdout = resource_true("flush-stdout");
    opt.run          = get_resource("run",NULL);
    compile_actions();

    intern_atoms();


    // allocate background color for window
    if (NULL == get_background_color(&win_bg)) {
        fprintf(stderr, "Failed to get background color\n");
        xcb_disconnect(c);
        exit(1);
    }

    // create window
    window = xcb_generate_id(c);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2];
    values[0] = win_bg.pixel;
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
}

void xcp_deinit () {
    XFreeColors(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &(win_bg.pixel), 1, 0);
    free_actions();
    free_resources();
    xcb_disconnect(c);
}
