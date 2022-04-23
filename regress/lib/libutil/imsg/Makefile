#	$OpenBSD$

REGRESS_TARGETS= run-regress

LDFLAGS= -lutil

CLEANFILES= ibuf_test ibuf_test.d

run-regress: ibuf_test
	${.OBJDIR}/ibuf_test

.include <bsd.regress.mk>
