#	$OpenBSD: Makefile,v 1.22 2002/08/12 10:46:35 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt

BINMODE?=2555

BINDIR=	/usr/bin
MAN=	ssh-agent.1

SRCS=	ssh-agent.c
#SRCS+=	ssh-pkcs11-client.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
