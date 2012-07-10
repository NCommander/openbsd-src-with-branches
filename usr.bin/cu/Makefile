# $OpenBSD$

PROG=	cu
SRCS=	cu.c command.c input.c

CDIAGFLAGS+= -Wall -W -Wno-unused-parameter

LDADD=  -levent
DPADD=  ${LIBEVENT}

.include <bsd.prog.mk>
