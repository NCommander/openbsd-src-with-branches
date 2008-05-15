#	$OpenBSD$

MAN=	mkboot.8
MANSUBDIR=socppc

.if ${MACHINE} == "socppc"
PROG=	mkboot
DPADD=	${LIBZ}
LDADD=	-lz
.else
NOPROG=
.endif

.include <bsd.prog.mk>
