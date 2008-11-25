#	$OpenBSD: Makefile,v 1.1 2008/11/24 23:34:41 uwe Exp $

PROG=		btd
SRCS=		bt.c bt_subr.c btd.c conf.c control.c db.c devinfo.c \
		hci.c log.c util.c
NOMAN=

LDADD+=		-levent -lusbhid
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btctl
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
