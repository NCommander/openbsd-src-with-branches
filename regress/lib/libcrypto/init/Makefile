#	$OpenBSD$
PROG=	init_pledge
SRCS+=	init_pledge.c
NOMAN=	yes

LDADD+=		-lcrypto -lutil
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} 

.include <bsd.regress.mk>
