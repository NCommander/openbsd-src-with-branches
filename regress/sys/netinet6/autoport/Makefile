#	$OpenBSD$

REGRESS_TARGETS=	run-regress-autoport
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-autoport:
	./autoport.pl 6 30000 31000 64

.include <bsd.regress.mk>
