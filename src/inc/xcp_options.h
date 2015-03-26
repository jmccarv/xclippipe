#ifndef __XCP_OPTIONS_H__
#define __XCP_OPTIONS_H__

#include <xcp_actions.h>

/* A place to cache resource settings so we don't
 * have to do a lot of resource lookups in the server.
 * Only frequently used options need be here.
 */
typedef struct xcp_options_t {
    int                 o_stdout;
    int                 flush_stdout;
    int                 debug;
    char                *nl;
    const char          *run;

    /* The following lists terminated with a .valid structure member of 0 */
    xcp_action_elem_t   *act_clipboard;
    xcp_action_elem_t   *act_primary;
    xcp_action_elem_t   *act_exit;
} xcp_options_t;

void       load_resources (int *argc, char **argv);
const char *get_resource  (const char *name, const char *class);
int        resource_true  (const char *name);

#endif