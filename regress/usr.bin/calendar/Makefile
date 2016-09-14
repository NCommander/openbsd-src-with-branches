#	$OpenBSD$

FILES?!=	ls ${.CURDIR}/*.out

CALENDAR?=	calendar
IN=		${.CURDIR}/in
OUT=		${.CURDIR}/${*}.out

.for f in ${FILES}
REGRESS_TARGETS+=${f:C|.*/||}
.endfor
.for f in ${FILES}
${f:C|.*/||}:
	@echo ${*}
	@${CALENDAR} -f ${IN} -t ${f:C|.*/||:C/\..*//} | diff ${OUT} -
.endfor

.PHONY:	${REGRESS_TARGETS}

.include <bsd.regress.mk>
