#	$OpenBSD: Makefile,v 1.1 2003/06/26 15:22:51 mickey Exp $

DIR=	usd/09.edtut
SRCS=	e.mac e0 e1 e2 e3 e4 e5 e6 e7
MACROS= -ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
