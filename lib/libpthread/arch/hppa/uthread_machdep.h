/*	$OpenBSD: uthread_machdep.h,v 1.5 2002/02/08 16:45:17 mickey Exp $	*/

struct _machdep_state {
	u_long	sp;
	u_long	fp;
	u_int64_t fpregs[32];
};
