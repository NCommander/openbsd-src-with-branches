#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:36:26 deraadt Exp $

.if ${MACHINE} == "sun3" || ${MACHINE} == "atari" || ${MACHINE} == "sparc"
PROG=	edlabel
CFLAGS+= -Wall
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

all: ${PROG}
.else
NOPROG=yes
.endif

NOMAN=
MANSUBDIR=sun3 sparc #atari

.include <bsd.prog.mk>
