#       $OpenBSD$

NOMAN=	# defined

PROG=	eventtest
CFLAGS+=-Wall
DPADD+=	${LIBEVENT}
LDADD+=	-levent

libevent-regress: ${PROG}
	@EVENT_SHOW_METHOD="yes" EVENT_NOPOLL="yes" ./eventtest
	@EVENT_SHOW_METHOD="yes" EVENT_NOKQUEUE="yes" ./eventtest

REGRESS_TARGETS=libevent-regress
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

