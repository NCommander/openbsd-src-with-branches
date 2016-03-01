#	$OpenBSD$
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.
#
# Run "cd /usr/src/lib/libedit && make obj && make depend" first.

PROG =		test_get
CPPFLAGS +=	-DWIDECHAR
CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
MAN =

.include <bsd.regress.mk>
