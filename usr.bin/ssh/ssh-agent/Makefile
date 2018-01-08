#	$OpenBSD: Makefile,v 1.27 2017/12/14 21:07:39 naddy Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ssh-pkcs11-client.c
SRCS+=	atomicio.c bufaux.c buffer.c compat.c fatal.c key.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt

BINMODE?=2555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

