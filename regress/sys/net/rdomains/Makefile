#	$OpenBSD$

DIR=	${.CURDIR}/../../net/rdomains

REGRESS_TARGETS=	run-regress-ifalocal
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-ifalocal:
	sh $(DIR)/ifalocal.sh

.include <bsd.regress.mk>
