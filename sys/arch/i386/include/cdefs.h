/*	$OpenBSD: cdefs.h,v 1.5.2.2 2001/10/31 03:01:12 nate Exp $	*/
/*	$NetBSD: cdefs.h,v 1.2 1995/03/23 20:10:26 jtc Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#if defined(__GNUC__) && defined(__STDC__)
#define __weak_alias(alias,sym)						\
	__asm__(".weak " __STRING(alias) " ; " __STRING(alias) " = " __STRING(sym))
#define __warn_references(sym,msg)					\
	__asm__(".section .gnu.warning." __STRING(sym) " ; .ascii \"" msg "\" ; .text")
#else
#define __indr_reference(sym,alias)
#define __warn_references(sym,msg)
#define __weak_alias(alias,sym)
#endif

#endif /* !_MACHINE_CDEFS_H_ */
