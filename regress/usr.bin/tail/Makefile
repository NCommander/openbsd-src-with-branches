# $OpenBSD$

REGRESS_TARGETS = follow-renames follow-overwrite-by-file follow-overwrite-data follow-removed-recreated follow-newsyslog
REGRESS_ROOT_TARGETS = follow-newsyslog

.for t in ${REGRESS_ROOT_TARGETS}
${t}:
	${SUDO} sh ${.CURDIR}/${t}.sh
.endfor

.for t in ${REGRESS_TARGETS}
${t}:
	sh ${.CURDIR}/${t}.sh
.endfor

.include <bsd.regress.mk>
