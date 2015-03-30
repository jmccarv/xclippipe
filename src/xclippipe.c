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

int xclippipe () {
    int done = 0;

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

    return 0;
}

int main (int argc, char **argv) {
    char *argv0 = strdup(argv[0]);
    program_name = basename(argv0);

    xcp_init(&argc, argv);

    if (!opt.o_stdout) 
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
