# $OpenBSD$

PROG=	ksu
NOMAN=
SRCS=	su.c
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase \
	-lwind -lpthread

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/su
