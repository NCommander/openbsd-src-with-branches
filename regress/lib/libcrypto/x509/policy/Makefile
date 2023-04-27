#	$OpenBSD: Makefile,v 1.5 2020/10/02 19:38:39 tb Exp $

PROGS =		policy

LDADD =	-lcrypto
DPADD =	${LIBCRYPTO}

LDADD_policy = ${CRYPTO_INT}

WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../../lib/libcrypto/x509
CFLAGS += 	-I${.CURDIR}/../../../../../lib/libcrypto/bytestring
CFLAGS += 	-DCERTSDIR=\"${.CURDIR}/../../../libcrypto/x509/policy\"

REGRESS_TARGETS =  policy-test

policy-test:	policy
	./policy 

.include "../../Makefile.inc"
.include <bsd.regress.mk>
