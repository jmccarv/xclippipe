#ifndef __XCP_XCLIPPIPE_H__
#define __XCP_XCLIPPIPE_H__

#include <xcb/xcb.h>

void debug              (const char *fmt, ...);
void request_selection  (xcb_atom_t selection);
void send_close_message ();

#endif
