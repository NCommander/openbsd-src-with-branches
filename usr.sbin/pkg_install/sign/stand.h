/* $OpenBSD$ */

/* provided to cater for BSD idiosyncrasies */

#ifndef __P
#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x) ()
#endif
#endif


#ifndef __GNUC__
#define __attribute__(x)
#endif
