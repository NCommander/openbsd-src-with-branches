#	$OpenBSD: Makefile,v 1.6 2015/11/03 11:03:10 semarie Exp $
PROG=	generic
SRCS+=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

.include <bsd.regress.mk>
