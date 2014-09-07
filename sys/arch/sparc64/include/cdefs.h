/*	$OpenBSD: cdefs.h,v 1.3 2005/11/24 20:46:48 deraadt Exp $	*/

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define __strong_alias(alias,sym)				\
	__asm__(".global " __STRING(alias) " ; "		\
	    __STRING(alias) " = " __STRING(sym))
#define __weak_alias(alias,sym)					\
	__asm__(".weak " __STRING(alias) " ; "			\
	    __STRING(alias) " = " __STRING(sym))
#define __warn_references(sym,msg)				\
	__asm__(".section .gnu.warning." __STRING(sym)		\
	    " ; .ascii \"" msg "\" ; .text")

#endif /* !_MACHINE_CDEFS_H_ */
