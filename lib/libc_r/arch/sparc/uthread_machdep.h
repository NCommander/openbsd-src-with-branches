/*	$OpenBSD$	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

struct _machdep_state {
	int	fp;		/* frame pointer */
	int	pc;		/* program counter */
};
