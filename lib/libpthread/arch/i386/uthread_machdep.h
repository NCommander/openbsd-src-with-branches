/*	$OpenBSD: uthread_machdep.h,v 1.9 2004/02/21 22:55:20 deraadt Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/npx.h>

struct _machdep_state {
	int		esp;
	/* must be 128-bit aligned */
	union savefpu	fpreg __attribute__ ((aligned (16)));
};
