#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:10 niklas Exp $


DIR=	usd/19.memacros
SRCS=	intro.me
MACROS=	-me

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
