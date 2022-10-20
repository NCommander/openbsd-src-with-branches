#	$OpenBSD: Makefile,v 1.2 2017/04/19 12:44:05 bluhm Exp $

REGRESS_TARGETS=	hit miss cleanup
CLEANFILES=		stamp-*

stamp-setup:
	${SUDO} pfctl -qt __regress_tbl -T add -f ${.CURDIR}/table.in
	date >$@

cleanup:
	rm -f stamp-setup
	${SUDO} pfctl -qt __regress_tbl -T kill

hit: stamp-setup
	for i in `cat ${.CURDIR}/table.hit`; do \
		echo -n "hit $$i "; \
		${SUDO} pfctl -qt __regress_tbl -T test $$i || exit 1; \
		echo success; \
	done; \
	exit 0

miss: stamp-setup
	for i in `cat ${.CURDIR}/table.miss`; do \
		echo -n "miss $$i "; \
		${SUDO} pfctl -qt __regress_tbl -T test $$i && exit 1; \
		echo success; \
	done; \
	exit 0

.PHONY: hit miss

.include <bsd.regress.mk>
