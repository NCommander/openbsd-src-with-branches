#	$OpenBSD$

TOPSRC=		${.CURDIR}/../../../../lib/libedit
TOPOBJ!!=	${MAKE} -C ${TOPSRC} -V .OBJDIR
CLAGS+=		-I${TOPSRC} -I${TOPOBJ}

.PHONY: generated

${PROG}: generated

generated:
	${MAKE} -C ${TOPSRC} generated
