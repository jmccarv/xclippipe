#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcp_options.h>

Display          *dpy;
xcb_connection_t *c;
xcb_screen_t     *screen;
xcb_window_t     window;
char             *program_name;

xcb_atom_t xcp_atom[5];
xcp_options_t opt = { 0, 0, 0, NULL, NULL, NULL, NULL, NULL };
