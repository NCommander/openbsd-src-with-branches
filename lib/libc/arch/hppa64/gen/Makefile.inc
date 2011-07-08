#	$OpenBSD: Makefile.inc,v 1.7 2009/04/21 09:34:50 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	infinity.c ldexp.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c

SRCS+=	alloca.c
