#       $OpenBSD$

PROG=   aes_test
LDADD=  -lcrypto
DPADD=  ${LIBCRYPTO}
WARNINGS=       Yes
CFLAGS+=        -DLIBRESSL_INTERNAL -DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
