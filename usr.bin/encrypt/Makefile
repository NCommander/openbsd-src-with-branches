#	$OpenBSD: Makefile,v 1.2 1996/08/26 08:41:23 downsj Exp $

PROG=	encrypt
SRCS= encrypt.c pwd_gensalt.c
.PATH: ${.CURDIR}/../passwd

LDADD = -lutil
LIBEXEC?=       /usr/libexec

LINKS=	${BINDIR}/encrypt ${LIBEXEC}/makekey
MLINKS=	encrypt.1 makekey.8

.include <bsd.prog.mk>
