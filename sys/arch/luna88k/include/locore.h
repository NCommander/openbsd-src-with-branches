/*	$OpenBSD: locore.h,v 1.3 2004/08/01 17:18:02 miod Exp $	*/

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

/*
 * C prototypes for various routines defined in locore_* and friends
 */

/* subr.S */

unsigned read_processor_identification_register(void);
int badaddr(vaddr_t addr, int size);
#define badwordaddr(x) badaddr(x, 4)
void set_cpu_number(unsigned number);
void doboot(void);

/* locore_c_routines.c */

unsigned getipl(void);

/* eh.S */

void sigsys(void);
void sigtrap(void);
void stepbpt(void);
void userbpt(void);
void syscall_handler(void);

#endif /* _MACHINE_LOCORE_H_ */
