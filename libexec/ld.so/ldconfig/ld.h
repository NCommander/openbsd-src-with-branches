/* $OpenBSD: $ */
/*
 * Header file to make code compatible with ELF version 
 * ldconfig was taken from the a.out ld.
 */
#include <link.h>
extern int n_search_dirs;
extern char **search_dirs;
char *xmalloc(int size);
char *concat __P((const char *, const char *, const char *));
#define PAGSIZ                 __LDPGSZ
