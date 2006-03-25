#	$OpenBSD: Makefile,v 1.2 2003/08/09 09:00:14 jmc Exp $

DIR=	usd/17.msmacros
SRCS=	ms fig1.pic
MACROS=	-ms

paper.ps: ${SRCS}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
