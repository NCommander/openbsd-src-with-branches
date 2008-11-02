#	$OpenBSD$

.PATH: ${.CURDIR}/../mrouted
CFLAGS+= -I${.CURDIR}/../mrouted

PROG=	map-mbone
SRCS=	igmp.c inet.c kern.c mapper.c
MAN=	map-mbone.8

.include <bsd.prog.mk>
