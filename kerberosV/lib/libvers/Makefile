# $OpenBSD$

LIB=	vers
SRCS=	print_version.c roken.h
CFLAGS+=-I. -I${KRB5DIR}/lib/roken

# skip installation
install:

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/vers ${KRB5DIR}/include ${.CURDIR}/../../include
