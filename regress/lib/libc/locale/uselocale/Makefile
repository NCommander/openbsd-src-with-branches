#	$OpenBSD$

PROG =		uselocale
LDFLAGS +=	-pthread

run-regress-${PROG}: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
