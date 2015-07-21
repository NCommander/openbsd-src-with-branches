#	$OpenBSD$
PROG=		radiusctl
SRCS=		radiusctl.c parser.c chap_ms.c
#MAN=		radiusctl.8
NOMAN=		#
CFLAGS+=	-Wall -Wextra -Wno-unused-parameter
LDADD+=		-lradius -lcrypto
DPADD+=		${LIBRADIUS} ${LIBCRYPTO}

.include <bsd.prog.mk>
