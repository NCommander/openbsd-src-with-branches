/*	$OpenBSD: cdefs.h,v 1.10 2006/01/10 00:04:04 millert Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define __strong_alias(alias,sym)			\
	__asm__(".global " __STRING(alias) " ; "	\
	    __STRING(alias) " = " __STRING(sym))
#define __weak_alias(alias,sym)				\
	__asm__(".weak " __STRING(alias) " ; "		\
	    __STRING(alias) " = " __STRING(sym))
#define __warn_references(sym,msg)			\
	__asm__(".section .gnu.warning." __STRING(sym)	\
	    " ; .ascii \"" msg "\" ; .text")

#endif /* !_MACHINE_CDEFS_H_ */
