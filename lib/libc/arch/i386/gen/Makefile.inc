#	$OpenBSD: Makefile.inc,v 1.5 2005/11/29 21:38:09 deraadt Exp $

SRCS+=	_setjmp.S alloca.S fabs.S infinity.c ldexp.c \
	modf.S nan.c setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S

LSRCS+= alloca.c
