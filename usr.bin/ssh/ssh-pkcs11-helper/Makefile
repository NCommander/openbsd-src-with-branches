#	$OpenBSD: Makefile,v 1.5 2018/01/08 15:18:46 markus Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-pkcs11-helper.c ssh-pkcs11.c
SRCS+=	atomicio.c bufaux.c buffer.c compat.c fatal.c readpass.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE}

PROG=	ssh-pkcs11-helper
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	ssh-pkcs11-helper.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
