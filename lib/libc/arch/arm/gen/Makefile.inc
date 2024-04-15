# $OpenBSD: Makefile.inc,v 1.16 2020/07/06 13:33:05 pirofti Exp $
# $NetBSD: Makefile.inc,v 1.6 2003/08/01 17:03:47 lukem Exp $

SRCS+=	divsi3.S fabs.c flt_rounds.c infinity.c
SRCS+=	ldexp.c modf.c nan.c usertc.c
SRCS+=	setjmp.S _setjmp.S sigsetjmp.S
