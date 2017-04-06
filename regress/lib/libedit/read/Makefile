#	$OpenBSD: Makefile,v 1.4 2016/04/11 21:15:11 schwarze Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.

REGRESS_TARGETS = getcmd read_char

CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
CLEANFILES +=	test_read_char test_getcmd out_getcmd.txt

depend:
	cd ${.CURDIR}/../../../../lib/libedit && make depend

read_char: depend test_read_char
	sh ${.CURDIR}/test_read_char.sh

getcmd: depend test_getcmd
	echo -n s | ./test_getcmd > out_getcmd.txt
	diff -u ${.CURDIR}/exp_getcmd.txt out_getcmd.txt

.include <bsd.regress.mk>
