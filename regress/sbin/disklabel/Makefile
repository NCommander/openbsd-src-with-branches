#	$OpenBSD$
#
# Regress tests for disklabel

REGRESS_TARGETS =	run-regress-disklabel-A

CLEANFILES	+= disklabel-A-output disklabel-A-f image

run-regress-disklabel-A:
	${SUDO} sh ${.CURDIR}/disklabel-A
	diff -up ${.CURDIR}/disklabel-A-expected disklabel-A-output

.include <bsd.regress.mk>
