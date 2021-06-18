#	$OpenBSD: Makefile,v 1.1 2021/05/19 17:16:51 deraadt Exp $

PROG=	dhcpleased
COPTS+=	-DSMALL
.PATH:  ${.CURDIR}/../../../sbin/dhcpleased
SRCS=	bpf.c checksum.c dhcpleased.c engine.c frontend.c

MAN=

LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
