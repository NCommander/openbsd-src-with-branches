#	$OpenBSD$

NOMAN=

.if (${MACHINE} == "i386")

PROG=		installboot

CPPFLAGS=	-I${.CURDIR}
LDADD=		-lutil
DPADD=		${LIBUTIL}

LDSTATIC=	-static

SRCS += installboot.c

.include "${.CURDIR}/${MACHINE}/Makefile.inc"

.else
NOPROG=
.endif

.include <bsd.prog.mk>
