# $OpenBSD$

.PATH:	${.CURDIR}/../tcpdump

PROG=	ndp
SRCS=	ndp.c gmt2local.c
MAN=	ndp.8 # ndp.4

CPPFLAGS+=-DINET6
CPPFLAGS+=-I${.CURDIR} -I${.CURDIR}/../tcpdump

.include <bsd.prog.mk>
