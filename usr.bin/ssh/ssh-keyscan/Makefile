#	$OpenBSD: Makefile,v 1.10 2017/12/14 21:07:39 naddy Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keyscan.c
SRCS+=	atomicio.c cleanup.c compat.c hostfile.c ssh_api.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXC} ${SRCS_KEXS} ${SRCS_KEY} \
	${SRCS_PKT} ${SRCS_UTL}
PROG=	ssh-keyscan
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+= -lcrypto -lz -lutil
DPADD+= ${LIBCRYPTO} ${LIBZ} ${LIBUTIL}

