#	$OpenBSD: Makefile,v 1.2 2018/07/07 23:16:24 beck Exp $

PROG=	unveil_test
SRCS=	syscalls.c
CFLAGS=	-Wall
NOMAN=	yes
LDADD=	-lutil
DPADD=	${LIBUTIL}

REGRESS_TARGETS =	test_normal
REGRESS_ROOT_TARGETS =	test_normal

test_normal: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
