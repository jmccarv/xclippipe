#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcp_global.h>

void debug (const char *fmt, ...) {
    va_list ap;

    if (!opt.debug) return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void *xzmalloc (size_t size) {
    void *ret;

    if (NULL == (ret = malloc(size))) {
        perror("Failed to malloc()");
        exit(1);
     }

    memset(ret, '\0', size);

    return ret;
}

char *xstrdup (const char *s) {
    char *ret;
    
    if (NULL == (ret = strdup(s))) {
        perror("strdup");
        exit(1);
    }

    return ret;
}
