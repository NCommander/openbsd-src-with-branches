/*	$OpenBSD: uthread_machdep.h,v 1.1 2001/09/10 20:00:14 jason Exp $	*/
/* Arutr Grabowski <art@openbsd.org>. Public domain. */

struct _machdep_state {
	long	fp;		/* frame pointer */
	long	pc;		/* program counter */
};
