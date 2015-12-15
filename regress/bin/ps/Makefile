# $OpenBSD: Makefile,v 1.1 2014/03/20 04:00:34 lteo Exp $

REGRESS_TARGETS=	valid_keywords invalid_keywords command

# This test ensures that ps(1) will actually accept the keywords it claims to
# accept, and that the list of keywords in ps/keyword.c are in ascending order
# (which is required by the bsearch(3)-based keyword lookup code)
valid_keywords:
	ps -o "$$(ps -L)" >/dev/null

invalid_keywords:
	if ps -o "bogus" >/dev/null 2>&1; then \
		false; \
	fi

command: shortsleep
	sh ${.CURDIR}/command.sh

cleandir:
	rm -f shortsleep

.include <bsd.regress.mk>
