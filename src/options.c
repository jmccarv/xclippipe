#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

#include <xcp_global.h>
#include <xcp_options.h>
#include <xcp_util.h>

static XrmOptionDescRec opTable[] = {
    { "-action.clipboard",  ".action.clipboard",    XrmoptionSepArg,    (XPointer) NULL  },
    { "-action.exit",       ".action.exit",         XrmoptionSepArg,    (XPointer) NULL  },
    { "-action.primary",    ".action.primary",      XrmoptionSepArg,    (XPointer) NULL  },
    { "-above",             ".above",               XrmoptionNoArg,     (XPointer) "on"  },
    { "+above",             ".above",               XrmoptionNoArg,     (XPointer) "off" },
    { "-background",        ".background",          XrmoptionSepArg,    (XPointer) NULL  },
    { "-bg",                ".background",          XrmoptionSepArg,    (XPointer) NULL  },
    { "-below",             ".below",               XrmoptionNoArg,     (XPointer) "on"  },
    { "+below",             ".below",               XrmoptionNoArg,     (XPointer) "off" },
    { "-borderless",        ".borderless",          XrmoptionNoArg,     (XPointer) "on"  },
    { "+borderless",        ".borderless",          XrmoptionNoArg,     (XPointer) "off" },
    { "-display",           ".display",             XrmoptionSepArg,    (XPointer) NULL  },
    { "-debug",             "._debug",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+debug",             "._debug",              XrmoptionNoArg,     (XPointer) "off" },
    { "-flush-stdout",      ".flush-stdout",        XrmoptionNoArg,     (XPointer) "on"  },
    { "+flush-stdout",      ".flush-stdout",        XrmoptionNoArg,     (XPointer) "off" },
    { "-geometry",          ".geometry",            XrmoptionSepArg,    (XPointer) NULL  },
    { "-g",                 ".geometry",            XrmoptionSepArg,    (XPointer) NULL  },
    { "-nl",                ".newline",             XrmoptionNoArg,     (XPointer) "on"  },
    { "+nl",                ".newline",             XrmoptionNoArg,     (XPointer) "off" },
    { "-newline",           ".newline",             XrmoptionNoArg,     (XPointer) "on"  },
    { "+newline",           ".newline",             XrmoptionNoArg,     (XPointer) "off"  },
    { "-run",               ".run",                 XrmoptionSepArg,    (XPointer) NULL  },
    { "-r",                 ".run",                 XrmoptionSepArg,    (XPointer) NULL  },
    { "+run",               ".run",                 XrmoptionNoArg,     (XPointer) ""    },
    { "+r",                 ".run",                 XrmoptionNoArg,     (XPointer) ""    },
    { "-stdout",            ".stdout",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+stdout",            ".stdout",              XrmoptionNoArg,     (XPointer) "off" },
    { "-s",                 ".stdout",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+s",                 ".stdout",              XrmoptionNoArg,     (XPointer) "off" },
    { "-sticky",            ".sticky",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+sticky",            ".sticky",              XrmoptionNoArg,     (XPointer) "off" },
    { "-title",             ".title",               XrmoptionSepArg,    (XPointer) NULL  },
    { "-help",              "._fullhelp",           XrmoptionIsArg,     (XPointer) NULL  },
    { "-h",                 "._help",               XrmoptionIsArg,     (XPointer) NULL  },
    { "-?",                 "._help",               XrmoptionIsArg,     (XPointer) NULL  },
    { "-version",           "._version",            XrmoptionIsArg,     (XPointer) NULL  },
};

typedef struct xcp_help_t {
    char *option;
    char *xresource;
    char *help_text;
} xcp_help_t;

static xcp_help_t xcp_help[] = {
    { "-action.clipboard",      "action.clipboard", "specify key and button presses which activate pasting from CLIPBOARD" },
    { "-action.exit",           "action.exit",      "specify key and button presses which exit the program" },
    { "-action.primary",        "action.primary",   "specify key and button presses which activate pasting from PRIMARY" },
    { "-/+above",               "above",            "turn on/off having window on top of other windows" },
    { "-bg color",              "background",       "background color" },
    { "-background color",      "background",       "background color" },
    { "-/+below",               "below",            "turn on/off having window below other windows" },
    { "-/+borderless",          "borderless",       "turn on/off window decorations" },
    { "-display displayname",   "display",          "X server to contact" },
    { "-/+debug",               "",                 "turn on/off debugging output to stderr" },
    { "-/+flush-stdout",        "flush-stdout",     "turn on/off calling fflush on stdout after each paste" },
    { "-g/-geometry geom",      "geometry",         "size and position of window" },
    { "-/+nl",                  "newline",          "turn on off appending a newline when pasting" },
    { "-/+newline",             "newline",          "turn on off appending a newline when pasting" },
    { "-r/-run command",        "run",              "run this program on each paste action, passing the selection contents on stdin.  Set to empty string '' to disable running any commands" },
    { "+r/+run",                "run",              "do not run any commands.  Same as -run ''" },
    { "-/+stdout",              "stdout",           "turn on/off pasting to stdout" },
    { "-/+s",                   "sticky",           "turn on off sticky mode - window will appear on every desktop" },
    { "-title title",           "title",            "set window title" },
    { "-?/-h",                  "",                 "this help" },
    { "-help",                  "",                 "full help including examples" },
    { "-version",               "",                 "output version and exit" },
};

XrmDatabase default_opts = NULL;
XrmDatabase opts = NULL;

option_defaults_t option_defaults = { 0, NULL };

static char *_option_defaults[] = {
    NULL, "-g", "100x100", "-bg", "black", "-title", "_pn_",
    "-stdout", "-flush-stdout", "-nl", 
    "-action.exit", "escape", 
    "-action.primary", "button2", 
    "-action.clipboard", "ctrl+v"
};

const char *get_resource (const char *name, const char *class) {
    if (!name)  return NULL;
    if (!class) class = name;

    char *res_name_buf  = xzmalloc(strlen(program_name)+strlen(name)+2);
    char *res_class_buf = xzmalloc(9+strlen(class)+2);

    sprintf(res_name_buf,  "%s.%s", program_name, name);
    sprintf(res_class_buf, "XClipPipe.%s", class);

    char *rec_type;
    XrmValue rec_val;

    XrmGetResource(opts, res_name_buf, res_class_buf, &rec_type, &rec_val);

    if (!rec_val.addr)
        XrmGetResource(default_opts, res_name_buf, res_class_buf, &rec_type, &rec_val);

    free(res_name_buf);
    free(res_class_buf);
    
    return(rec_val.addr);
}

int resource_true (const char *name) {
    const char *res = get_resource(name, NULL);

    if (res && 0 == strcmp("on",res))
        return 1;

    return 0;
}

void load_resources (int *argc, char **argv) {
    int i;
    char *s;
    int def_argc;
    char **def_argv;

    def_argc = option_defaults.argc = sizeof(_option_defaults) / sizeof(char *);
    option_defaults.argv = xzmalloc(sizeof(char *) * def_argc);
    def_argv             = xzmalloc(sizeof(char *) * def_argc);

    for (i=1; i < def_argc; i++) {
        if (0 == (strcmp("_pn_", _option_defaults[i]))) {
            s = xstrdup(program_name);
        } else {
            s = xstrdup(_option_defaults[i]);
        }
        def_argv[i] = option_defaults.argv[i] = s;
    }

    XrmParseCommand(&default_opts, opTable, sizeof(opTable)/sizeof(XrmOptionDescRec), program_name, &def_argc, def_argv);
    free(def_argv);

    opts = XrmGetStringDatabase(XResourceManagerString(dpy));
    XrmParseCommand(&opts, opTable, sizeof(opTable)/sizeof(XrmOptionDescRec), program_name, argc, argv);

    XrmSetDatabase(dpy, opts);
}

void free_resources () {
    int i;
    
    for (i=1; i < option_defaults.argc; i++) {
        if (option_defaults.argv[i]) free(option_defaults.argv[i]);
    }

    XrmDestroyDatabase(opts);
    XrmDestroyDatabase(default_opts);

    free(option_defaults.argv);
}

void short_help (int include_resource_name) {
    int i;

    printf("%s - paste from X clipboard to stdout and/or a command\n", program_name);
    printf("usage: %s [OPTIONS]\n\n", program_name);
    printf("OPTIONS:\n");

    if (include_resource_name) {
        printf("  %-22s %-18s Description\n", "Command Line Parameter", "X resource");
        for (i=0; i < sizeof(xcp_help)/sizeof(xcp_help_t); i++) {
            printf("  %-22s %-18s %s\n", xcp_help[i].option, xcp_help[i].xresource, xcp_help[i].help_text);
        }
    } else {
        for (i=0; i < sizeof(xcp_help)/sizeof(xcp_help_t); i++) {
            printf("  %-22s %s\n", xcp_help[i].option, xcp_help[i].help_text);
        }
    }
}

void usage () {
    short_help(0);
    exit(0);
}

void full_help () {
    char *p = program_name;
    int i;
    short_help(1);

    printf(
        "\n%s pastes the contents of an X selection (PRIMARY and/or CLIPBOARD)\n"
        "to its stdout and/or the stdin of another process\n\n"
        "%s creates a window and waits for keyboard or mouse button presses\n"
        "to tell it what action to take.  There are three possible actions:\n"
        "  action.exit - exit the program\n"
        "  action.clipboard - paste from the X CLIPBOARD selection (usually ctrl-v)\n"
        "  action.primary - paste from the X PRIMARY selection (usually middle mouse button)\n\n"
        "You can configure what keys and buttons are bound to these actions on the\n"
        "command line or in your .Xresources.\n\n"
        "Key and button names are case insensitive\n\n"
        "\nEXAMPLES\n"
        , p, p
    );

    printf(
        "1.  Paste to stdout.\n"
        "    Paste from CLIPBOARD on ctrl-v\n"
        "    and from PRIMARY on mouse button 2 (middle mouse button)\n"
        "    Exit on Escape key\n\n"
        "    %s -stdout -action.clipboard ctrl+v -action.primary button2 -action.exit escape\n\n"
        "2.  For each paste event, run a process (cat) sending the selection data to its stdin\n"
        "    Paste from PRIMARY on mouse button 2 or ctrl-alt-p\n"
        "    Paste from CLIPBOARD on mouse button 3 or ctrl-v\n"
        "    Exit on ctrl-d or Escape or mouse button 1\n\n"
        "    %s -run 'cat >> $HOME/%s.log' -action.clipboard 'ctrl+v|button3' -action.primary 'button2|ctrl+mod2+p' -action.exit 'escape|button1'\n\n"
        "3.  Paste to stdout and explicitely disable any 'run' commands that might be in your .Xresources\n"
        "    Disable appending a newline to output, set the background color to green,\n"
        "    turn off window manager decorations (borders), and run above other windows\n\n"
        "    %s -stdout -run '' +nl -bg green -borderless -above\n\n"
        "4.  Use debugging to see what keys and buttons you're pressing to help know what to pass for actions\n\n"
        "    %s +stdout +run -debug\n\n"
        , p, p, p, p, p
    );

    printf("Default Parameters:\n ");
    for (i=1; i < option_defaults.argc; i++)
        printf(" %s",option_defaults.argv[i]);
    printf("\n\n");
    printf("The defaults are used when no other resource of the same name\n"
           "is found either from the command line or from the\n"
           "X server (.Xresources, etc)\n"
          );

    exit(0);
}
