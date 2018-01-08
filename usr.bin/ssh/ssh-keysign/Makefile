#	$OpenBSD: Makefile,v 1.12 2017/12/14 21:07:39 naddy Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c
SRCS+=	atomicio.c cleanup.c fatal.c msg.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_PKT} \
	${SRCS_UTL}
PROG=	ssh-keysign
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
