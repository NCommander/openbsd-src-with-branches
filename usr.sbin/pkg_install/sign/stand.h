/* $OpenBSD: stand.h,v 1.1 1999/09/27 21:40:04 espie Exp $ */

/* provided to cater for BSD idiosyncrasies */

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#ifndef __P
#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x) ()
#endif
#endif

#if defined(BSD4_4)
#include <err.h>
#else
extern void set_program_name __P((const char * name));
extern void warn __P((const char *fmt, ...));
extern void warnx __P((const char *fmt, ...));
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif
