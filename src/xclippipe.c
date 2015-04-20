/*
 * Copyright (c) 2015, Jason McCarver (slam@parasite.cc) All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <signal.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>

#include <xcp_global.h>
#include <xcp_init.h>
#include <xcp_util.h>

void send_close_message () {
    xcb_client_message_event_t event;
    
    event.response_type  = XCB_CLIENT_MESSAGE;
    event.format         = 32;
    event.window         = window;
    event.type           = xcp_atom[WM_PROTOCOLS];
    event.data.data32[0] = xcp_atom[WM_DELETE_WINDOW];

    xcb_send_event(c, False, window, XCB_EVENT_MASK_NO_EVENT, (char *)&event);
    xcb_flush(c);
}

char *get_modifier_state(uint16_t state) {
    /* terrible, I know */
    static char modifier_state[64];

    modifier_state[0] = '\0';

    if (state & XCB_KEY_BUT_MASK_SHIFT)    strcat(modifier_state,"shift+");
    if (state & XCB_KEY_BUT_MASK_LOCK)     strcat(modifier_state,"lock+");
    if (state & XCB_KEY_BUT_MASK_CONTROL)  strcat(modifier_state,"ctrl+");
    if (state & XCB_KEY_BUT_MASK_MOD_1)    strcat(modifier_state,"mod1+");
    if (state & XCB_KEY_BUT_MASK_MOD_2)    strcat(modifier_state,"mod2+");
    if (state & XCB_KEY_BUT_MASK_MOD_3)    strcat(modifier_state,"mod3+");
    if (state & XCB_KEY_BUT_MASK_MOD_4)    strcat(modifier_state,"mod4+");
    if (state & XCB_KEY_BUT_MASK_MOD_5)    strcat(modifier_state,"mod5+");
    
    return modifier_state;
}

void ev_button_press (xcb_button_press_event_t *event) {
    xcp_action_elem_t act = { NULL, event->detail, event->state, 0 };

    debug("%sbutton%d\n",get_modifier_state(event->state),event->detail);
    do_action(&act);
}

void ev_key_press (xcb_key_press_event_t *event) {
    KeySym ks = XkbKeycodeToKeysym(dpy, event->detail, 0, 0);

    char *ch = XKeysymToString(ks);

    if (IsModifierKey(ks)) return;

    debug("%s%s\n", get_modifier_state(event->state),ch);

    xcp_action_elem_t act = { ch, 0, event->state, 0 };
    do_action(&act);
}

int xclippipe () {
    int done = 0;

    xcb_generic_event_t *event;
    while (!done && (event = xcb_wait_for_event(c))) {
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

    return 0;
}

int main (int argc, char **argv) {
    char *argv0 = strdup(argv[0]);
    program_name = basename(argv0);

    xcp_init(&argc, argv);

    if (!resource_true("stdout"))
        fclose(stdout);

    struct sigaction sigact_chld;
    memset(&sigact_chld, '\0', sizeof(struct sigaction));
    sigact_chld.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP;
    sigact_chld.sa_handler = SIG_DFL;
    sigemptyset(&sigact_chld.sa_mask);
    sigaction (SIGCHLD, &sigact_chld, NULL);

    int ret = xclippipe();

    xcp_deinit();
    free(argv0);

    return ret;
}
