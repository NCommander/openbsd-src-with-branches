# $OpenBSD$

PROG=	dhtest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-Werror

CFLAGS+=	-I${.CURDIR}/../../../../lib/libcsi

SRCS=	dhtest.c

.PATH:  ${.CURDIR}/../../../../lib/libcsi

SRCS+=  csi.c csi_dh.c csi_dh_groups.c csi_util.c

.include <bsd.regress.mk>
