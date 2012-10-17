# $OpenBSD: Makefile,v 1.4 2011/02/05 18:26:14 schwarze Exp $

REGRESS_TARGETS=vspace

TBL=/usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} > ${.TARGET}
.endfor

.include <bsd.regress.mk>
