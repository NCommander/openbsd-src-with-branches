#	$OpenBSD$

SRCS+=	__main.c _setjmp.S alloca.S fabs.S frexp.S infinity.c isinf.c
SRCS+=	ldexp.S modf.S setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
