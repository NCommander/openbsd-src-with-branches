#	$OpenBSD: Makefile,v 1.1 2018/04/07 13:54:46 schwarze Exp $

PROGS =	x509attribute x509name
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS=regress-x509attribute regress-x509name
CLEANFILES+=	x509name.result

regress-x509attribute: x509attribute
	./x509attribute

regress-x509name: x509name
	./x509name > x509name.result
	diff -u ${.CURDIR}/x509name.expected x509name.result

.include <bsd.regress.mk>
