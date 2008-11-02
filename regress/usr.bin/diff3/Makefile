# $OpenBSD: Makefile,v 1.4 2005/06/13 19:22:02 otto Exp $

.PHONY: ${REGRESS_TARGETS}

REGRESS_TARGETS+=t1
t1:
	merge -p /etc/motd /etc/motd /etc/motd | diff /etc/motd -

.include <bsd.regress.mk>
