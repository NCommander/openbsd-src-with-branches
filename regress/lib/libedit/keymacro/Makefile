#	$OpenBSD: Makefile,v 1.2 2016/04/11 21:15:11 schwarze Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.

PROG =		test_get
CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
MAN =

depend:
	cd ${.CURDIR}/../../../../lib/libedit && make depend
${PROG}: depend
regress: ${PROG}

.include <bsd.regress.mk>
