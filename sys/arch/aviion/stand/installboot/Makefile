#	$OpenBSD$

MAN=		installboot.8
MANSUBDIR=	aviion

.if ${MACHINE} == "aviion"
PROG=		installboot
BINDIR=		/usr/mdec
DPADD=		${LIBUTIL}
LDADD=		-lutil
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
