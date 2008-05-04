#       $OpenBSD: Makefile,v 1.4 2003/09/23 08:52:04 markus Exp $

PROG=	eventtest
LDADD=	-levent
DPADD=	${LIBEVENT}

CFLAGS+= -I${.CURDIR}/../../../../lib/libevent

REGRESS_TARGETS=	eventtest-select eventtest-kqueue eventtest-poll
.PHONY: ${REGRESS_TARGETS}

eventtest-kqueue: ${PROG}
	@echo libevent using: kqueue
	@echo
	@EVENT_NOPOLL=yes EVENT_NOSELECT=yes ./${PROG}
	@echo

eventtest-select: ${PROG}
	@echo libevent using: select
	@echo
	@EVENT_NOPOLL=yes EVENT_NOKQUEUE=yes ./${PROG}
	@echo

eventtest-poll: ${PROG}
	@echo libevent using: poll
	@echo
	@EVENT_NOSELECT=yes EVENT_NOKQUEUE=yes ./${PROG}
	@echo

.include <bsd.regress.mk>
