#	$OpenBSD$

.PATH: ${.CURDIR}/../ssh
CPPFLAGS += -I${.CURDIR}/../ssh

SRCS=	signify.c
SRCS+=	ed25519.c fe25519.c ge25519.c sc25519.c smult_curve25519_ref.c
SRCS+=	blocks.c hash.c verify.c

PROG=	signify

LDADD=  -lutil
DPADD=  ${LIBUTIL}

COPTS+=	-Wall

.include <bsd.prog.mk>
