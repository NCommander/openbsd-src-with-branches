/*	$OpenBSD$	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/reg.h>

struct _machdep_state {
	int		esp;
	struct fpreg	fpreg;
};

