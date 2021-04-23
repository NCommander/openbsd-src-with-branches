/* $OpenBSD$ */

/*
 * machine/setjmp.h: machine dependent setjmp-related information.
 */

#define	_JBLEN		256	/* sp, ra, [f]s0-11, magic val, sigmask */
#define	_JB_SIGMASK	27
