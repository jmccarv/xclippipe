#ifndef __XCP_GLOBAL_H__
#define __XCP_GLOBAL_H__

#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcp_options.h>

typedef enum xcp_atom_idx_t {
    WM_PROTOCOLS = 0,
    WM_DELETE_WINDOW = 1,
    CLIPBOARD = 2,
    _XCP_CLIP = 3,
    _MOTIF_WM_HINTS = 4
} xcp_atom_idx_t;

extern Display          *dpy;
extern xcb_connection_t *c;
extern xcb_screen_t     *screen;
extern xcb_window_t     window;

extern xcb_atom_t xcp_atom[4];
extern xcp_options_t opt;

#endif
