#       $OpenBSD: Makefile,v 1.2 2003/08/09 09:00:14 jmc Exp $

DIR=	usd/01.begin
SRCS=	u.mac u0 u1 u2 u3 u4 u5
MACROS= -ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
