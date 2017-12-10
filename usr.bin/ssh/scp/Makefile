#	$OpenBSD: Makefile,v 1.17 2017/07/10 14:09:59 espie Exp $

.PATH:		${.CURDIR}/..

SRCS=	scp.c
SRCS+=	atomicio.c cleanup.c fatal.c log.c misc.c progressmeter.c \
	sshbuf-getput-basic.c sshbuf.c ssherr.c uidswap.c utf8.c xmalloc.c

PROG=	scp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>
