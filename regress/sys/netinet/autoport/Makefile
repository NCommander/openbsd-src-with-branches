#	$OpenBSD$

DIR=	../../netinet6/autoport

REGRESS_TARGETS=	run-regress-autoport
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-autoport:
	$(DIR)/autoport.pl 4 30000 31000 64

.include <bsd.regress.mk>
