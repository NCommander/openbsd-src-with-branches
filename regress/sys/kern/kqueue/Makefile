#	$OpenBSD: Makefile,v 1.3 2002/03/02 21:48:05 art Exp $

PROG=	kqueue-test
SRCS=	kqueue-pipe.c kqueue-fork.c main.c kqueue-process.c kqueue-random.c

kq-pipe: ${PROG}
	./${PROG} -p
kq-fork: ${PROG}
	./${PROG} -f
kq-process: ${PROG}
	./${PROG} -P
kq-random: ${PROG}
	./${PROG} -r

REGRESSTARGETS=kq-pipe kq-fork kq-process kq-random
.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
