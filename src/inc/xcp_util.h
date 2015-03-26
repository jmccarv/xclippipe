#ifndef __XCP_UTIL_H__
#define __XCP_UTIL_H__

#include <xcb/xcb.h>

void debug     (const char *fmt, ...);
void *xzmalloc (size_t size);
char *xstrdup  (const char *s);

#endif
