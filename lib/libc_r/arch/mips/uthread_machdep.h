/*
 * OpenBSD/mips machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.3 1999/01/17 23:49:49 d Exp $
 */

#include <machine/regnum.h>
#include <machine/signal.h>

/* floating point state is saved by setjmp/longjmp */

#define _thread_machdep_save_float_state(thr) 		/* no need */
#define _thread_machdep_restore_float_state(thr) 	/* no need */

/* initialise the jmpbuf stack frame so it continues from entry */
#define _thread_machdep_thread_create(thr, entry, pattr)	\
	{							\
	    struct sigcontext *j = &(thr)->saved_jmp_buf;	\
								\
	    /* initialise to sane values */			\
	    _thread_machdep_setjmp(j);				\
	    /* entry */						\
	    j->sc_regs[RA] = j->sc_pc; /* for gdb */		\
	    j->sc_pc = (int)entry;				\
	    /* stack */						\
	    j->sc_regs[SP] = (int) (thr)->stack->base		\
				+ (thr)->stack->size		\
				- sizeof(double);		\
	}

#define _thread_machdep_longjmp(a,v)	longjmp(a,v)
#define _thread_machdep_setjmp(a)	setjmp(a)

typedef jmp_buf _machdep_jmp_buf;

struct _machdep_struct {
	/* nothing needed */
};
