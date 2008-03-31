#	$OpenBSD: Makefile.inc,v 1.4 2005/02/01 15:12:30 millert Exp $

SRCS+=	_setjmp.S fabs.S infinity.c isinf.c ldexp.S \
	modf.S setjmp.S udiv.S urem.S sigsetjmp.S

SRCS+= alloca.S
LSRCS+= alloca.c
