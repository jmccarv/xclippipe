#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

#include <xcp_global.h>
#include <xcp_options.h>

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
    { "-debug",             ".debug",               XrmoptionNoArg,     (XPointer) "on"  },
    { "+debug",             ".debug",               XrmoptionNoArg,     (XPointer) "off" },
    { "-flush-stdout",      ".flush-stdout",        XrmoptionNoArg,     (XPointer) "on"  },
    { "+flush-stdout",      ".flush-stdout",        XrmoptionNoArg,     (XPointer) "off" },
    { "-geometry",          ".geometry",            XrmoptionSepArg,    (XPointer) NULL  },
    { "-g",                 ".geometry",            XrmoptionSepArg,    (XPointer) NULL  },
    { "-nl",                ".newline",             XrmoptionNoArg,     (XPointer) "on"  },
    { "+nl",                ".newline",             XrmoptionNoArg,     (XPointer) "off" },
    { "-run",               ".run",                 XrmoptionSepArg,    (XPointer) NULL  },
    { "-r",                 ".run",                 XrmoptionSepArg,    (XPointer) NULL  },
    { "-stdout",            ".stdout",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+stdout",            ".stdout",              XrmoptionNoArg,     (XPointer) "off" },
    { "-s",                 ".stdout",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+s",                 ".stdout",              XrmoptionNoArg,     (XPointer) "off" },
    { "-sticky",            ".sticky",              XrmoptionNoArg,     (XPointer) "on"  },
    { "+sticky",            ".sticky",              XrmoptionNoArg,     (XPointer) "off" },
    { "-title",             ".title",               XrmoptionSepArg,    (XPointer) NULL  },
    { "-help",              NULL,                   XrmoptionSkipArg,   (XPointer) NULL  },
    { "-version",           NULL,                   XrmoptionSkipArg,   (XPointer) NULL  },
    { "-?",                 NULL,                   XrmoptionSkipArg,   (XPointer) NULL  },
};

XrmDatabase default_opts = NULL;
XrmDatabase opts = NULL;

char *option_defaults[] = {
    NULL, "-g", "100x100", "-bg", "black", "-title", "xclippipe", 
    "-stdout", "-flush-stdout", "-nl", 
    "-action.exit", "escape", 
    "-action.primary", "button2", 
    "-action.clipboard", "ctrl+v"
};


#define RES_BUF_SIZE 1024
char res_name_buf[RES_BUF_SIZE];
char res_class_buf[RES_BUF_SIZE];
const char *get_resource (const char *name, const char *class) {
    char res_name_buf[RES_BUF_SIZE]  = "xclippipe.";
    char res_class_buf[RES_BUF_SIZE] = "XClipPipe.";
    char *rec_type;
    XrmValue rec_val;

    strncat(res_name_buf,  name,                 RES_BUF_SIZE-strlen(res_name_buf)-1);
    strncat(res_class_buf, class ? class : name, RES_BUF_SIZE-strlen(res_class_buf)-1);

    XrmGetResource(opts, res_name_buf, res_class_buf, &rec_type, &rec_val);

    if (!rec_val.addr)
        XrmGetResource(default_opts, res_name_buf, res_class_buf, &rec_type, &rec_val);
    
    return(rec_val.addr);
}

int resource_true (const char *name) {
    const char *res = get_resource(name, NULL);

    if (res && 0 == strcmp("on",res))
        return 1;

    return 0;
}

void load_resources (int *argc, char **argv) {
    opts = XrmGetStringDatabase(XResourceManagerString(dpy));

    int c = sizeof(option_defaults)/sizeof(char *);
    XrmParseCommand(&default_opts, opTable, sizeof(opTable)/sizeof(XrmOptionDescRec), "xclippipe", &c, option_defaults);

    XrmParseCommand(&opts, opTable, sizeof(opTable)/sizeof(XrmOptionDescRec), "xclippipe", argc, argv);

    XrmSetDatabase(dpy, opts);
}
