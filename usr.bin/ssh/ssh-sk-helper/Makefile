#	$OpenBSD: Makefile,v 1.9 2019/09/06 05:59:41 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-sk-helper.c
SRCS+=	atomicio.c fatal.c cleanup.c msg.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE} ${SRCS_SK}

PROG=	ssh-sk-helper

BINDIR=	/usr/libexec
#MAN=	ssh-sk-helper.8
NOMAN=1

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
