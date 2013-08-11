# $OpenBSD$

REGRESS_TARGETS=template_v9 template_v10

template_v9:
	${.CURDIR}/template.pl 9 | cmp -s ${.CURDIR}/template.v9 /dev/stdin

template_v10:
	${.CURDIR}/template.pl 10 | cmp -s ${.CURDIR}/template.v10 /dev/stdin

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
