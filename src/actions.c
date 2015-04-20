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

/* action callbacks */
void act_clipboard () {
    debug("pasting from CLIPBOARD\n");
    request_selection(xcp_atom[CLIPBOARD]);
}

void act_primary () {
    debug("pasting from PRIMARY\n");
    request_selection(XCB_ATOM_PRIMARY);
}

void act_exit() {
    debug("exiting\n");
    send_close_message();
}



void do_child_command(xcb_get_property_reply_t *prop, int len) {
    FILE *fh;
    int  rc;

    struct sigaction sigact_chld;
    const char *cmd = get_resource("run",NULL);

    memset(&sigact_chld, '\0', sizeof(struct sigaction));
    sigact_chld.sa_handler = SIG_DFL;
    sigemptyset(&sigact_chld.sa_mask);
    sigaction (SIGCHLD, &sigact_chld, NULL);

    debug("running command: '%s'\n", cmd);

    if (NULL == (fh = popen(cmd, "w"))) {
        fprintf(stderr, "Failed to run command '%s': ", cmd);
        perror(NULL);
        return;
    }
    
    fprintf(fh,"%.*s%s", len,(char *)xcb_get_property_value(prop), opt.nl);

    if (-1 == (rc = pclose(fh))) {
        fprintf(stderr, "Command '%s' failed upon pclose(): ", cmd);
        perror(NULL);
        return;
    }

    debug("command exited with status: %d\n", WEXITSTATUS(rc));

    if (rc) {
        fprintf(stderr, "Command '%s' terminated with error code: %d", cmd, WEXITSTATUS(rc));
        
        if (WIFSIGNALED(rc))
            fprintf(stderr, " (signaled with %d)", WTERMSIG(rc));

        if (WCOREDUMP(rc))
            fprintf(stderr, " (core dumped)");

        fprintf(stderr,"\n");
    }
}

void run_command (xcb_get_property_reply_t *prop) {
    pid_t child;
    const char *cmd = get_resource("run",NULL);

    if (! (cmd && strlen(cmd))) return;
    if (!prop)                          return;

    int len   = xcb_get_property_value_length(prop);
    if (len < 1) return;

    switch(child = fork()) {
        case -1:
            fprintf(stderr, "Failed to fork() to run command '%s': %s\n", cmd, strerror(errno));
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
        debug("pasting string: '%.*s'\n", xcb_get_property_value_length(prop),(char *)xcb_get_property_value(prop));
        if (resource_true("stdout")) {
            printf("%.*s%s", xcb_get_property_value_length(prop),(char *)xcb_get_property_value(prop), opt.nl);

            if (resource_true("flush_stdout")) 
                fflush(stdout);
        }

        if (get_resource("run",NULL)) {
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


int action_matches (xcp_action_elem_t *action, xcp_action_elem_t *event) {
    if (   event->ks_string
        && action->ks_string
        && ((!action->mod_mask) || (action->mod_mask & event->mod_mask))
        && 0 == strcasecmp(event->ks_string, action->ks_string))
    {
        return 1;
    }

    if (   event->button
        && action->button == event->button
        && ((!action->mod_mask) || (action->mod_mask & event->mod_mask)))
    {
        return 1;
    }

    return 0;
}

void do_action (xcp_action_elem_t *event) {
    xcp_action_elem_t *p = opt.action_handlers;

    for (; p; p = p->next) {
        if (action_matches(p, event)) {
            p->handler();
        }
    }
}


void add_action (xcp_action_elem_t *action) {
    xcp_action_elem_t *prev = NULL;
    xcp_action_elem_t *p = opt.action_handlers;

    if (!p) {
        action->next = NULL;
        opt.action_handlers = action;
        return;
    }

    /* We want the entries with the most nr_mask_bits first in the list 
     * so that we always check the most specific bindings first followed
     * by the less specific.
     *
     * If we didn't do this we would match actions that the user didn't
     * intend us to.  For example:
     *   action.primary = 'ctrl+button2'
     *   action.exit    = 'ctrl+shift+button2'
     * If we weren't sorting this list and the action.primary came first
     * in the list we would never exit on ctrl+shift+button2 as we would always
     * match action.primary instead.
     */
    for (; p; p = p->next) {
        if (action->nr_mask_bits >= p->nr_mask_bits) {
            action->next = p;

            if (prev)
                prev->next = action;
            else
                opt.action_handlers = action;

            break;
        }
        prev = p;
    }

    if (!p && prev) {
        prev->next = action;
    }
}

#define ADD_MASK(x) { \
    action->mod_mask |= x; \
    nr_bits++; \
}
void compile_action (const char *resource, xcp_action_callback_t handler) {
    char *buf = NULL;
    char *p, *ps, *q, *qs, *next;
    int nr_bits;
    xcp_action_elem_t *action;

    if (! (resource && handler)) return;
    
    buf = xstrdup(resource);
    //debug("compiling action: %s\n", buf);

    q = strtok_r(buf, "|", &qs);
    while (q && (p = strtok_r(q, "+-", &ps))) {
        nr_bits = 0;
        action = xzmalloc(sizeof(xcp_action_elem_t));

        do {
            next = strtok_r(NULL, "+-", &ps);

            if (next) {
                // p is a modifier key
                if (0 == strcasecmp(p, "shift")) {
                    ADD_MASK(XCB_MOD_MASK_SHIFT);
                } else if (0 == strcasecmp(p, "lock")) {
                    ADD_MASK(XCB_MOD_MASK_LOCK);
                } else if (0 == strcasecmp(p, "ctrl")) {
                    ADD_MASK(XCB_MOD_MASK_CONTROL);
                } else if (0 == strcasecmp(p, "mod1")) {
                    ADD_MASK(XCB_MOD_MASK_1);
                } else if (0 == strcasecmp(p, "mod2")) {
                    ADD_MASK(XCB_MOD_MASK_2);
                } else if (0 == strcasecmp(p, "mod3")) {
                    ADD_MASK(XCB_MOD_MASK_3);
                } else if (0 == strcasecmp(p, "mod4")) {
                    ADD_MASK(XCB_MOD_MASK_4);
                } else if (0 == strcasecmp(p, "mod5")) {
                    ADD_MASK(XCB_MOD_MASK_5);
                } else {
                    fprintf(stderr, "Unknown modifier: '%s' for %s\n", p, resource);
                }

            } else if (0 == strncasecmp(p, "button", 6)) {
                // p is a mouse button
                if (strlen(p) == 7) {
                    action->button = atoi(p+6);
                } else {
                    fprintf(stderr, "Unknown button: '%s' for %s\n", p, resource);
                }

            } else {
                // p is the key being modified
                action->ks_string = xstrdup(p);
            }
        } while ((p = next));

        action->nr_mask_bits = nr_bits;
        action->handler = handler;
        add_action(action);

        q = strtok_r(NULL, "|", &qs);
    }

    free(buf);
}

void compile_actions () {
    compile_action(get_resource("action.clipboard", NULL), act_clipboard);
    compile_action(get_resource("action.primary", NULL), act_primary);
    compile_action(get_resource("action.exit", NULL), act_exit);
}

void free_actions () {
    xcp_action_elem_t *p = opt.action_handlers;
    xcp_action_elem_t *q = NULL;

    for (; p; q = p, p = p->next) {
        if (q) free (q);
        if (p->ks_string) free(p->ks_string);
    }

    if (q) free (q);

    opt.action_handlers = NULL;
}
