#	$OpenBSD: Makefile,v 1.1 2001/05/05 06:46:59 mickey Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	smm/11.timedop
SRCS=	timed.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
