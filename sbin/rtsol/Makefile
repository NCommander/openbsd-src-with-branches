#	$OpenBSD$

PROG=	rtsol
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

CPPFLAGS+=-DINET6
LDADD+=	-lkvm
DPADD+=	${LIBKVM}

#MAN=	rtsold.8
NOMAN=	yes

.PATH:	${.CURDIR}/../../usr.sbin/rtsold

.include <bsd.prog.mk>
