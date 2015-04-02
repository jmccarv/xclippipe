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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include <xcp_actions.h>
#include <xcp_global.h>
#include <xcp_xclippipe.h>
#include <xcp_util.h>

typedef enum xcp_action_t {
    XCP_ACTION_NONE = 0,
    XCP_ACTION_CLIPBOARD = 1,
    XCP_ACTION_PRIMARY = 2,
    XCP_ACTION_EXIT = 3
} xcp_action_t;

void do_child_command(xcb_get_property_reply_t *prop, int len) {
    FILE *fh;
    int  rc;

    struct sigaction sigact_chld;

    memset(&sigact_chld, '\0', sizeof(struct sigaction));
    sigact_chld.sa_handler = SIG_DFL;
    sigemptyset(&sigact_chld.sa_mask);
    sigaction (SIGCHLD, &sigact_chld, NULL);

    debug("running command: '%s'\n", opt.run);

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

    debug("command exited with status: %d\n", WEXITSTATUS(rc));

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
        fprintf(stderr, "No property reply\n");
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
        debug("pasting string: '%*s'\n", xcb_get_property_value_length(prop),(char *)xcb_get_property_value(prop));
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
            debug("pasting from CLIPBOARD\n");
            request_selection(xcp_atom[CLIPBOARD]);
            break;

        case XCP_ACTION_PRIMARY:
            debug("pasting from PRIMARY\n");
            request_selection(XCB_ATOM_PRIMARY);
            break;

        case XCP_ACTION_EXIT:
            debug("exiting\n");
            send_close_message();
            break;

        case XCP_ACTION_NONE:
            break;
    }
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

    *act = xzmalloc(sizeof(xcp_action_elem_t) * (count+1));

    xcp_action_elem_t *a = *act;

    q = strtok_r(buf, "|", &qs);
    while (q) {
        p = strtok_r(q, "+-", &ps);
        while (p) {
            next = strtok_r(NULL, "+-", &ps);

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
