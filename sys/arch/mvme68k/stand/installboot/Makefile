#	$OpenBSD$

MAN=	installboot.8
MANSUBDIR=mvme68k

.if ${MACHINE} == "mvme68k"
PROG=	installboot
BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
