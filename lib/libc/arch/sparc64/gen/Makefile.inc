#	$OpenBSD: Makefile.inc,v 1.12 2012/04/12 16:14:09 deraadt Exp $

SRCS+=	_setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpclassifyl.c \
	fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c infinity.c isfinitel.c \
	isinfl.c isnanl.c isnormall.c ldexp.c modf.S \
	mul.S nan.c saveregs.S setjmp.S signbitl.c sigsetjmp.S umul.S
