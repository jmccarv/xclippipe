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
    const char          *run;
    char                *nl;

    /* The following lists terminated with a .valid structure member of 0 */
    xcp_action_elem_t   *act_clipboard;
    xcp_action_elem_t   *act_primary;
    xcp_action_elem_t   *act_exit;
} xcp_options_t;

typedef struct option_defaults_t {
    int  argc;
    char **argv;
} option_defaults_t;


void       load_resources           (int *argc, char **argv);
void       free_resources           ();
const char *get_resource            (const char *name, const char *class);
const char *get_default_resource    (const char *name);
int        resource_true            (const char *name);
void       usage                    (int status);
void       full_help                ();
void       version                  ();

#endif
