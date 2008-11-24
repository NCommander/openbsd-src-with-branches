#	$OpenBSD$

.PATH:		${.CURDIR}/../btd

PROG=		btctl
SRCS=		btctl.c conf.c log.c parse.y
NOMAN=

LDADD+=		-lbluetooth -lsdp -lusbhid
LDFLAGS+=	-L/usr/local/lib
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btd -I/usr/local/include
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
