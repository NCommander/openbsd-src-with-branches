#	$OpenBSD$

.PATH:	${.CURDIR}/../tcpdchk
CFLAGS+=-I${.CURDIR}/../tcpdchk

PROG=	tcpdmatch
MAN=	tcpdmatch.8

SRCS=	inetcf.c scaffold.c tcpdmatch.c

DPADD=	${LIBWRAP}
LDADD=	-lwrap

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
