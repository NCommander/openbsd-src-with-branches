/*	$OpenBSD$	*/

#include <sys/types.h>
#include <machine/fpu.h>

struct _machdep_state {
	long	rsp;
	/* must be 128-bit aligned */
	struct savefpu   fpreg __attribute__ ((aligned (16)));
};
