#	$OpenBSD: Makefile,v 1.1 2001/05/05 06:46:59 mickey Exp $
#	@(#)Makefile	8.2 (Berkeley) 5/11/94

DIR=	smm/12.timed
SRCS=	timed.ms
.if make(install)
SRCS+=	date loop time timed.ms unused
.endif
MACROS=	-ms

paper.ps: ${SRCS}
	${SOELIM} -I${.CURDIR} ${.ALLSRC} | ${TBL} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
