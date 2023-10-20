#	$OpenBSD: Makefile,v 1.3 2023/04/28 16:18:17 tb Exp $

PROGS =		policy

LDADD =	${CRYPTO_INT}
DPADD =	${LIBCRYPTO}

WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../../lib/libcrypto/x509
CFLAGS += 	-I${.CURDIR}/../../../../../lib/libcrypto/bytestring
CFLAGS += 	-DCERTSDIR=\"${.CURDIR}/../../../libcrypto/x509/policy\"

.include "../../Makefile.inc"
.include <bsd.regress.mk>
