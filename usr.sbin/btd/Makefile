#	$OpenBSD$

PROG=		btd
SRCS=		bt.c btd.c conf.c control.c db.c devinfo.c \
		hci.c log.c sdp.c util.c
NOMAN=

LDADD+=		-levent -lbluetooth -lsdp -lusbhid
LDFLAGS+=	-L/usr/local/lib
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btctl -I/usr/local/include
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
