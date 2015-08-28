/*	$OpenBSD: cdefs.h,v 1.5 2013/01/05 11:20:56 miod Exp $ */

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_M88K_CDEFS_H_
#define	_M88K_CDEFS_H_

#define __strong_alias(alias,sym)			\
	__asm__(".global " __STRING(alias) " ; "	\
	    __STRING(alias) " = " __STRING(sym))
#define __weak_alias(alias,sym)				\
	__asm__(".weak " __STRING(alias) " ; "		\
	    __STRING(alias) " = " __STRING(sym))
#define __warn_references(sym,msg)			\
	__asm__(".section .gnu.warning." __STRING(sym)	\
	    " ; .ascii \"" msg "\" ; .text")

#endif /* _M88K_CDEFS_H_ */
