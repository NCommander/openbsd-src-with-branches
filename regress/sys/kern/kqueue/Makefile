#	$OpenBSD: Makefile,v 1.1 2002/01/07 00:01:00 provos Exp $

PROG=	kqueue-test
SRCS= kqueue-pipe.c kqueue-fork.c main.c

kq-pipe: ${PROG}
	./${PROG} -p
kq-fork: ${PROG}
	./${PROG} -f

REGRESSTARGETS=kq-pipe kq-fork
.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
