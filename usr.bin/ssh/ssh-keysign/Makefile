#	$OpenBSD: Makefile,v 1.21 2001/06/27 19:29:16 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keysign
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/libexec
NOMAN=yes

SRCS=	ssh-keysign.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
