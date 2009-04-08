# $OpenBSD: Makefile.inc,v 1.8 2005/11/29 21:38:08 deraadt Exp $
# $NetBSD: Makefile.inc,v 1.6 2003/08/01 17:03:47 lukem Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S divsi3.S fabs.c flt_rounds.c infinity.c \
	nan.c
SRCS+=	setjmp.S _setjmp.S sigsetjmp.S modf.c ldexp.c

SRCS+=	alloca.S
LSRCS+= alloca.c
