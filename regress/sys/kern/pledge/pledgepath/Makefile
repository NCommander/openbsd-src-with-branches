#	$OpenBSD: Makefile,v 1.8 2017/02/21 15:46:25 tb Exp $
PROG=	pledgepath_test
SRCS+=	syscalls.c
NOMAN=	yes

LDADD+=		-lutil
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} 

.include <bsd.regress.mk>
