#	$OpenBSD: Makefile,v 1.6 2018/11/06 06:56:17 tb Exp $

SUBDIR= \
	addsub \
	general \
	mont \
	rand

PROGS+= bn_cmp

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Wall -Wundef -Werror

REGRESS_TARGETS += run-bn_cmp
run-bn_cmp: bn_cmp
	./bn_cmp

.include <bsd.regress.mk>
