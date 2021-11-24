#	$OpenBSD: Makefile,v 1.3 2021/11/23 01:14:26 deraadt Exp $

PROGS=		poll_close pollnval pollretval
LDADD=		-lpthread
WARNINGS=	yes

run-regress-poll_close:
	# Disabled until poll(2) uses kqueue as backend.
	@echo DISABLED

.include <bsd.regress.mk>
