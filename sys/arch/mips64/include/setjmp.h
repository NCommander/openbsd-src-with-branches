/*	$OpenBSD: setjmp.h,v 1.3 2011/03/23 16:54:36 pirofti Exp $	*/

/* Public domain */

#ifndef _MIPS64_SETJMP_H_
#define _MIPS64_SETJMP_H_

#define	_JB_MASK	(1 * REGSZ)
#define	_JB_PC		(2 * REGSZ)
#define	_JB_REGS	(3 * REGSZ)
#define	_JB_FPREGS	(37 * REGSZ)

#define	_JBLEN	83		/* size, in longs, of a jmp_buf */

#endif /* !_MIPS64_SETJMP_H_ */
