#	$OpenBSD: Makefile,v 1.18 2014/03/18 22:36:29 miod Exp $

PROG=	kbd

SRCS=	main.c kbd_wscons.c
CFLAGS+=-DNOKVM
.PATH:  ${.CURDIR}/../../../sbin/kbd

.include <bsd.prog.mk>
