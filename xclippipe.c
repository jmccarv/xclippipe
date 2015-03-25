#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
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

Display          *dpy;
xcb_connection_t *c;
xcb_screen_t     *screen;
xcb_window_t     window;

typedef enum xcp_action_t {
    XCP_ACTION_NONE = 0,
    XCP_ACTION_CLIPBOARD = 1,
    XCP_ACTION_PRIMARY = 2,
    XCP_ACTION_EXIT = 3
} xcp_action_t;

typedef struct xcp_action_elem_t {
    char            *ks_string;
    int             button;
    xcb_keycode_t   mod_mask;
    int             valid;
} xcp_action_elem_t;

/* A place to cache resource settings so we don't
 * have to do a lot of resource lookups in the server.
 * Only frequently used options need be here.
 */
typedef struct xcp_options_t {
    int                 o_stdout;
    int                 flush_stdout;
    int                 debug;
    char                *nl;
    const char          *run;
    /* The following are null-terminated lists */
    xcp_action_elem_t   *act_clipboard;
    xcp_action_elem_t   *act_primary;
    xcp_action_elem_t   *act_exit;
} xcp_options_t;

xcp_options_t opt = { 0, 0, 0, NULL, NULL };


static XrmOptionDescRec opTable[] = {
    { "-action.clipboard",  ".action.clipboard",    XrmoptionSepArg,    (XPointer) NULL  },
    { "-action.exit",       ".action.exit",         XrmoptionSepArg,    (XPointer) NULL  },
    { "-action.primary",    ".action.primary",      XrmoptionSepArg,    (XPointer) NULL  },
    { "-above",             ".above",               XrmoptionNoArg,     (XPointer) "on"  },
    { "+above",             ".above",               XrmoptionNoArg,     (XPointer) "off" },
    { "-background",        ".background",          XrmoptionSepArg,    (XPointer) NULL  },
    { "-bg",                ".background",          XrmoptionSepArg,    (XPointer) NULL  },
    { "-below",             ".below",               XrmoptionNoArg,     (XPointer) "on"  },
    { "+below",             ".below",               XrmoptionNoArg,     (XPointer) "off" },
    { "-borderless",        ".borderless",          XrmoptionNoArg,     (XPointer) "on"  },
    { "+borderless",        ".borderless",          XrmoptionNoArg,     (XPointer) "off" },
    { "-display",           ".display",             XrmoptionSepArg,    (XPointer) NULL  },
    { "-debug",             ".debug",               XrmoptionNoArg,     (XPointer) "on"  },
    { "+debug",             ".debug",               XrmoptionNoArg,     (XPointer) "off" },
    { "-flush-stdout",      ".flush-stdout",        XrmoptionNoArg,     (XPointer) "on"  },
    { "+flush-stdout",      ".flush-stdout",        XrmoptionNoArg,     (XPointer) "off" },
    { "-geometry",          ".geometry",            XrmoptionSepArg,    (XPointer) NULL  },
    { "-g",                 ".geometry",            XrmoptionSepArg,    (XPointer) NULL  },
    { "-nl",                ".newline",             XrmoptionNoArg,     (XPointer) "on"  },
    { "+nl",                ".newline",             XrmoptionNoArg,     (XPointer) "off" },
    { "-run",               ".run",                 XrmoptionSepArg,    (XPointer) NULL  },
    { "-r",                 ".run",                 XrmoptionSepArg,    (XPointer) NULL  },
    { "-stdout",            ".stdout",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+stdout",            ".stdout",              XrmoptionNoArg,     (XPointer) "off" },
    { "-s",                 ".stdout",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+s",                 ".stdout",              XrmoptionNoArg,     (XPointer) "off" },
    { "-sticky",            ".sticky",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+sticky",            ".sticky",              XrmoptionNoArg,     (XPointer) "off" },
    { "-title",             ".title",               XrmoptionSepArg,    (XPointer) NULL  },
    { "-help",              NULL,                   XrmoptionSkipArg,   (XPointer) NULL  },
    { "-version",           NULL,                   XrmoptionSkipArg,   (XPointer) NULL  },
    { "-?",                 NULL,                   XrmoptionSkipArg,   (XPointer) NULL  },
};

XrmDatabase default_opts = NULL;
XrmDatabase opts = NULL;

char *option_defaults[] = {
    NULL, "-g", "100x100", "-bg", "black", "-title", "xclippipe", 
    "-stdout", "-flush-stdout", "-nl", 
    "-action.exit", "escape", 
    "-action.primary", "button2", 
    "-action.clipboard", "ctrl+v"
};

void debug (const char *fmt, ...) {
    va_list ap;

    if (!opt.debug) return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

#define RES_BUF_SIZE 1024
char res_name_buf[RES_BUF_SIZE];
char res_class_buf[RES_BUF_SIZE];
const char *get_resource (const char *name, const char *class) {
    char res_name_buf[RES_BUF_SIZE]  = "xclippipe.";
    char res_class_buf[RES_BUF_SIZE] = "XClipPipe.";
    char *rec_type;
    XrmValue rec_val;

    strncat(res_name_buf,  name,                 RES_BUF_SIZE-strlen(res_name_buf)-1);
    strncat(res_class_buf, class ? class : name, RES_BUF_SIZE-strlen(res_class_buf)-1);

    XrmGetResource(opts, res_name_buf, res_class_buf, &rec_type, &rec_val);

    if (!rec_val.addr)
        XrmGetResource(default_opts, res_name_buf, res_class_buf, &rec_type, &rec_val);
    
    return(rec_val.addr);
}

int resource_true (const char *name) {
    const char *res = get_resource(name, NULL);

    if (res && 0 == strcmp("on",res))
        return 1;

    return 0;
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

void do_child_command(xcb_get_property_reply_t *prop, int len) {
    FILE *fh;
    int  rc;

    struct sigaction sigact_chld;

    memset(&sigact_chld, '\0', sizeof(struct sigaction));
    sigact_chld.sa_handler = SIG_DFL;
    sigemptyset(&sigact_chld.sa_mask);
    sigaction (SIGCHLD, &sigact_chld, NULL);

    if (NULL == (fh = popen(opt.run, "w"))) {
        fprintf(stderr, "Failed to run command '%s': ", opt.run);
        perror(NULL);
        return;
    }
    
    fprintf(fh,"%*s%s", len,(char *)xcb_get_property_value(prop), opt.nl);

    if (-1 == (rc = pclose(fh))) {
        fprintf(stderr, "Command '%s' failed upon pclose(): ", opt.run);
        perror(NULL);
        return;
    }

    if (rc) {
        fprintf(stderr, "Command '%s' terminated with error code: %d", opt.run, WEXITSTATUS(rc));
        
        if (WIFSIGNALED(rc))
            fprintf(stderr, " (signaled with %d)", WTERMSIG(rc));

        if (WCOREDUMP(rc))
            fprintf(stderr, " (core dumped)");

        fprintf(stderr,"\n");
    }
}

void run_command (xcb_get_property_reply_t *prop) {
    pid_t child;

    if (! (opt.run && strlen(opt.run))) return;
    if (!prop)                          return;

    int len   = xcb_get_property_value_length(prop);
    if (len < 1) return;

    switch(child = fork()) {
        case -1:
            fprintf(stderr, "Failed to fork() to run command '%s': %s\n", opt.run, strerror(errno));
            break;

        case 0:
            break;

        default:
            debug("forked() child: %d\n", child);
            return;
    }

    fclose(stdin);
    fclose(stdout);
    do_child_command(prop, len);

    exit(0);
}

void ev_selection_notify (xcb_selection_notify_event_t *event) {
    xcb_get_property_cookie_t prop_cookie;
    xcb_get_property_reply_t  *prop;

    prop_cookie = xcb_get_property(c, False, window, event->property, XCB_ATOM_STRING, 0, UINT32_MAX);
    prop        = xcb_get_property_reply(c, prop_cookie, NULL);
    
    if (!prop) {
        fprintf(stderr, "No propery reply\n");
        return;
    }

    /*
    debug("resp type=%d format=%d seq=%d length=%d type=%d bytes_after=%u value_len=%u\n", 
           prop->type, prop->format, prop->sequence, prop->length, prop->type, prop->bytes_after, prop->value_len);
    */

    if (prop->value_len < 1) {
        fprintf(stderr, "Property reply was zero length\n");
        free(prop);
        return;
    }
    

    if (prop->type == XCB_ATOM_STRING && prop->format == 8) {
        debug("got string: %*s\n", xcb_get_property_value_length(prop),(char *)xcb_get_property_value(prop));
        if (opt.o_stdout) {
            printf("%*s%s", xcb_get_property_value_length(prop),(char *)xcb_get_property_value(prop), opt.nl);

            if (opt.flush_stdout) 
                fflush(stdout);
        }

        if (opt.run) {
            run_command(prop);
        }

    } else {
        fprintf(stderr, "Got something that's not a string; ignoring\n");
    }

    if (prop->bytes_after) {
        fprintf(stderr, "Did not receive all the data from the clipboard, output truncated\n");
    }

    free(prop);

    xcb_delete_property(c, window, event->property);
}

void request_selection (xcb_atom_t selection) {
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;

    cookie = xcb_get_selection_owner(c, selection);
    reply  = xcb_get_selection_owner_reply(c, cookie, NULL);

    if (reply->owner == None) {
        fprintf(stderr, "No selection owner\n");
        return;
    }

    xcb_convert_selection(c, window, selection, XCB_ATOM_STRING, xcp_atom[_XCP_CLIP], CurrentTime);
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

int check_action (xcp_action_elem_t *action_list, xcp_action_elem_t *needle) {
    for (; action_list->valid; action_list++) {
        if (   needle->ks_string
            && action_list->ks_string
            && ((!action_list->mod_mask) || (action_list->mod_mask & needle->mod_mask))
            && 0 == strcasecmp(needle->ks_string, action_list->ks_string)) 
        {
            return 1;

        } else if (needle->button && action_list->button == needle->button) {
            return 1;
        }
    }

    return 0;
}

xcp_action_t find_action (xcp_action_elem_t *needle) {
    if (check_action(opt.act_clipboard, needle))
        return XCP_ACTION_CLIPBOARD;

    else if (check_action(opt.act_primary, needle))
        return XCP_ACTION_PRIMARY;

    else if (check_action(opt.act_exit, needle))
        return XCP_ACTION_EXIT;

    return XCP_ACTION_NONE;
}

void do_action (xcp_action_elem_t *act) {
    switch (find_action(act)) {
        case XCP_ACTION_CLIPBOARD:
            request_selection(xcp_atom[CLIPBOARD]);
            break;

        case XCP_ACTION_PRIMARY:
            request_selection(XCB_ATOM_PRIMARY);
            break;

        case XCP_ACTION_EXIT:
            send_close_message();
            break;

        case XCP_ACTION_NONE:
            break;
    }
}

void ev_button_press (xcb_button_press_event_t *event) {
    xcp_action_elem_t act = { NULL, event->detail, 0, 0 };

    debug("button%d\n",event->detail);
    do_action(&act);
}

void ev_key_press (xcb_key_press_event_t *event) {
    KeySym ks = XkbKeycodeToKeysym(dpy, event->detail, 0, 0);

    char *ch = XKeysymToString(ks);

    if (IsModifierKey(ks)) return;

    if (event->state & XCB_KEY_BUT_MASK_SHIFT)    debug("shift+");
    if (event->state & XCB_KEY_BUT_MASK_LOCK)     debug("lock+");
    if (event->state & XCB_KEY_BUT_MASK_CONTROL)  debug("ctrl+");
    if (event->state & XCB_KEY_BUT_MASK_MOD_1)    debug("mod1+");
    if (event->state & XCB_KEY_BUT_MASK_MOD_2)    debug("mod2+");
    if (event->state & XCB_KEY_BUT_MASK_MOD_3)    debug("mod3+");
    if (event->state & XCB_KEY_BUT_MASK_MOD_4)    debug("mod4+");
    if (event->state & XCB_KEY_BUT_MASK_MOD_5)    debug("mod5+");
    debug("%s\n", ch);

    xcp_action_elem_t act = { ch, 0, event->state, 0 };
    do_action(&act);
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
        //debug("Got Event\n");
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

void compile_action (xcp_action_elem_t **act, const char *resource) {
    const char *val = get_resource(resource,NULL);
    int  count;
    char *buf = NULL;
    char *p, *ps, *q, *qs, *next;

    if (! (act && val)) return;
    
    buf = strdup(val);

    for (count=1, p=buf; *p; p++)
        if (*p == '|') count++; 

    if (NULL == (*act = malloc(sizeof(xcp_action_elem_t) * (count+1)))) {
        perror("Failed to malloc()");
        exit(1);
    }
    memset(*act, '\0', sizeof(xcp_action_elem_t) * (count+1));

    xcp_action_elem_t *a = *act;

    q = strtok_r(buf, "|", &qs);
    while (q) {
        p = strtok_r(q, "+", &ps);
        while (p) {
            next = strtok_r(NULL, "+", &ps);

            if (next) {
                // p is a modifier key
                if (0 == strcasecmp(p, "shift")) {
                    a->mod_mask |= XCB_MOD_MASK_SHIFT;
                } else if (0 == strcasecmp(p, "lock")) {
                    a->mod_mask |= XCB_MOD_MASK_LOCK;
                } else if (0 == strcasecmp(p, "ctrl")) {
                    a->mod_mask |= XCB_MOD_MASK_CONTROL;
                } else if (0 == strcasecmp(p, "mod1")) {
                    a->mod_mask |= XCB_MOD_MASK_1;
                } else if (0 == strcasecmp(p, "mod2")) {
                    a->mod_mask |= XCB_MOD_MASK_2;
                } else if (0 == strcasecmp(p, "mod3")) {
                    a->mod_mask |= XCB_MOD_MASK_3;
                } else if (0 == strcasecmp(p, "mod4")) {
                    a->mod_mask |= XCB_MOD_MASK_4;
                } else if (0 == strcasecmp(p, "mod5")) {
                    a->mod_mask |= XCB_MOD_MASK_5;
                } else {
                    fprintf(stderr, "Unknown modifier: '%s' for %s\n", p, resource);
                }

            } else if (0 == strncasecmp(p, "button", 6)) {
                // p is a mouse button
                a->button = atoi(p+6);
            } else {
                // p is the key being modified
                a->ks_string = strdup(p);
            }

            p=next;
        }

        q = strtok_r(NULL, "|", &qs);
        a->valid = 1;
        a++;
    }

    for (a=*act; a->valid; a++) {
        if (a->ks_string)
            debug("%s: key: %s  mod: 0x%04x\n", resource, a->ks_string, a->mod_mask);
        else
            debug("%s: button: %d\n", resource, a->button);
    }

    free(buf);
}

void compile_actions () {
    compile_action(&opt.act_clipboard, "action.clipboard");
    compile_action(&opt.act_primary,   "action.primary");
    compile_action(&opt.act_exit,      "action.exit");
}

void free_action (xcp_action_elem_t **act) {
    if (!act) return;

    xcp_action_elem_t *a = *act;
    for (; a->valid; a++)
        free(a->ks_string);

    *act = NULL;
    free(*act);
}

void free_actions () {
    free_action(&opt.act_clipboard);
    free_action(&opt.act_primary);
    free_action(&opt.act_exit);
}

int main (int argc, char **argv) {
    dpy = XOpenDisplay(NULL);
    struct sigaction sigact_chld;

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
    opt.nl           = resource_true("newline") ? "\n" : "";
    opt.debug        = resource_true("debug");
    opt.o_stdout     = resource_true("stdout");
    opt.flush_stdout = resource_true("flush-stdout");
    opt.run          = get_resource("run",NULL);
    compile_actions();

    if (!opt.o_stdout) 
        fclose(stdout);

    memset(&sigact_chld, '\0', sizeof(struct sigaction));
    sigact_chld.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP;
    sigact_chld.sa_handler = SIG_DFL;
    sigemptyset(&sigact_chld.sa_mask);
    sigaction (SIGCHLD, &sigact_chld, NULL);

    xclippipe();

    free_actions();
    xcb_disconnect(c);
    return 0;
}
