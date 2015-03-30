#ifndef __XCP_ACTIONS_H__
#define __XCP_ACTIONS_H__

#include <xcb/xcb.h>

typedef struct xcp_action_elem_t {
    char            *ks_string;
    int             button;
    xcb_keycode_t   mod_mask;
    int             valid;
} xcp_action_elem_t;

void compile_actions ();
void do_action (xcp_action_elem_t *action);
void free_actions ();
void ev_selection_notify (xcb_selection_notify_event_t *event);

#endif
