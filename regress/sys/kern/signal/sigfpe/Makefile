#	$OpenBSD$

PROG=	sigfpe

# XXX - don't want the compiler to optimize away our tests
CFLAGS=   -O0 ${PIPE} ${DEBUG}

REGRESSTARGETS=	divzero oflow

divzero:
	./${PROG} -z

oflow:
	./${PROG} -o

.include <bsd.regress.mk>
