#	$OpenBSD: Makefile,v 1.3 2021/03/20 16:36:52 kn Exp $

PROG=	dhcpleased
.PATH:  ${.CURDIR}/../../../sbin/dhcpleased
SRCS=	bpf.c checksum.c control.c dhcpleased.c engine.c frontend.c log.c

MAN=

LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
