#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            debug("Pasting from CLIPBOARD\n");
            request_selection(xcp_atom[CLIPBOARD]);
            break;

        case XCP_ACTION_PRIMARY:
            debug("Pasting from PRIMARY\n");
            request_selection(XCB_ATOM_PRIMARY);
            break;

        case XCP_ACTION_EXIT:
            debug("Exiting\n");
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
        p = strtok_r(q, "+", &ps);
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
