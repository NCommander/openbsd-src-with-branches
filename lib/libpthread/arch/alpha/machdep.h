/* ==== machdep.h ============================================================
 * Copyright (c) 1994 Chris Provenzano (proven@athena.mit.edu) and
 * Ken Raeburn (raeburn@mit.edu).
 *
 * engine-alpha-osf1.h,v 1.4.4.1 1995/12/13 05:41:42 proven Exp
 *
 */

#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/cdefs.h>

/* The first machine dependent functions are the SEMAPHORES needing
   the test and set instruction.

   On the Alpha, the actual values here are irrelevant; they just have
   to be different.  */
#define SEMAPHORE_CLEAR 0
#define SEMAPHORE_SET   1

#if 0
#define SEMAPHORE_TEST_AND_SET(lock)    			\
({ int *_sem_lock = (lock), locked, old;			\
   asm ("mb" : : : "memory");					\
   do { asm ("ldl_l %0,%1" : "=r" (old) : "m" (*_sem_lock));	\
	/* ?? if (old != SEMAPHORE_CLEAR) break; */		\
	asm ("stl_c %0,%1" : "=r" (locked), "=m" (*_sem_lock)	\
			   : "0" (SEMAPHORE_SET));		\
   } while (!locked);						\
   asm ("mb" : : : "memory");					\
   old == SEMAPHORE_CLEAR; })

#define SEMAPHORE_RESET(lock)           \
({ int *_sem_lock = (lock);		\
   *_sem_lock = SEMAPHORE_CLEAR;	\
   asm ("mb" : : : "memory"); })
#endif

/*
 * New types
 */
typedef int semaphore;

/*
 * sigset_t macros
 */
#define	SIG_ANY(sig)		(sig)
#define	SIGMAX 31

/*
 * New Strutures
 */
struct machdep_pthread {
    void        		*(*start_routine)(void *);
    void        		*start_argument;
    void        		*machdep_stack;
    struct itimerval		machdep_timer;
    unsigned long		machdep_istate[11];
    unsigned long     		machdep_fstate[9];
};

/*
 * Static machdep_pthread initialization values.
 * For initial thread only.
 */
#define MACHDEP_PTHREAD_INIT    \
	{ NULL, NULL, NULL, { { 0, 0 }, { 0, 100000 } }, 0 }

/*
 * Minimum stack size
 */
#define PTHREAD_STACK_MIN	2048

/*
 * Some fd flag defines that are necessary to distinguish between posix
 * behavior and bsd4.3 behavior.
 */
#define __FD_NONBLOCK 		O_NONBLOCK

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(PTHREAD_KERNEL)

#define __machdep_stack_get(x)      (x)->machdep_stack
#define __machdep_stack_set(x, y)   (x)->machdep_stack = y
#define __machdep_stack_repl(x, y)                          \
{                                                           \
    if (stack = __machdep_stack_get(x)) {                   \
        __machdep_stack_free(stack);                        \
    }                                                       \
    __machdep_stack_set(x, y);                              \
}

void *  __machdep_stack_alloc       __P_((size_t));
void    __machdep_stack_free        __P_((void *));

int machdep_save_state      __P_((void));

int __machdep_save_int_state __P_((unsigned long *));
void __machdep_restore_int_state __P_((unsigned long *));
void __machdep_save_fp_state __P_((unsigned long *));
void __machdep_restore_fp_state __P_((unsigned long *));

#endif

__END_DECLS
