# $OpenBSD: Makefile,v 1.12 2015/01/28 17:30:37 schwarze Exp $

TBL = /usr/local/bin/tbl

SKIP_TMAN ?= ALL SILENT

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include "../Makefile.inc"
