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

#include <xcp_motif_window_hints.h>
#include <xcp_global.h>
#include <xcp_init.h>

void debug (const char *fmt, ...) {
    va_list ap;

    if (!opt.debug) return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
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

    return ret;
}
