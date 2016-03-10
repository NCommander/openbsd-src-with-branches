# $OpenBSD: Makefile,v 1.2 2014/08/14 01:58:51 schwarze Exp $

REGRESS_TARGETS	= break vspace
LINT_TARGETS	= break

# groff-1.22.3 defect:
# - Starting a table in next-line scope confuses font handling,
#   and in the case of .TP indentation as well.

SKIP_GROFF	= break

TBL=/usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include <bsd.regress.mk>
