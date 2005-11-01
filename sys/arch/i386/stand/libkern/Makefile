#	$OpenBSD$

.include "${.CURDIR}/../Makefile.inc"

LIB=	kern

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..

.PATH:	${S}/lib/libkern

NOPROFILE=noprofile
NOPIC=nopic

# kern routines
SRCS+=	moddi3.c qdivrem.c strlcpy.c strlcat.c

all: ${KERNLIB}

install:

.include <bsd.lib.mk>
