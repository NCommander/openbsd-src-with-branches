#	$OpenBSD$

PROG=	tcpdchk
MAN=	tcpdchk.8

SRCS=	inetcf.c scaffold.c tcpdchk.c

DPADD=	${LIBWRAP}
LDADD=	-lwrap

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
