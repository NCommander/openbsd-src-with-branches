#	$OpenBSD: Makefile,v 1.8 2010/01/13 01:40:16 djm Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-pkcs11-helper
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
NOMAN=	yes

SRCS=	ssh-pkcs11-helper.c ssh-pkcs11.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
