#	$OpenBSD$

MAN=	installboot.8
MANSUBDIR=mvme88k

.if ${MACHINE} == "mvme88k"
PROG=	installboot
BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
# Need this to work in the miniroot
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>

