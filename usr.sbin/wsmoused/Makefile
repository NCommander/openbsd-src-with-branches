#	$OpenBSD$	

.if ${MACHINE} == "i386"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386

.include <bsd.prog.mk>

