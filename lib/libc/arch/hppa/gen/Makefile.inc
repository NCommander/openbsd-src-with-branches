#	$OpenBSD: Makefile.inc,v 1.3 2002/02/06 18:50:26 mickey Exp $

#SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	setjmp.S
SRCS+=	fabs.c frexp.c ldexp.c
SRCS+=	isnan.c isinf.c infinity.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c
