/*	$OpenBSD: cdefs.h,v 1.2 1999/01/27 04:46:04 imp Exp $	*/

#ifndef _MIPS_CDEFS_H_
#define	_MIPS_CDEFS_H_

#define	_C_LABEL(x)	_STRING(x)

#define __weak_alias(alias,sym) \
    __asm__(".weak " __STRING(alias) " ; " __STRING(alias) " = " __STRING(sym))
#define	__warn_references(sym,msg) \
    __asm__(".section .gnu.warning." __STRING(sym) " ; .ascii \"" msg "\" ; .text")
#define	__indr_references(sym,msg)	/* nothing */

#endif /* !_MIPS_CDEFS_H_ */
