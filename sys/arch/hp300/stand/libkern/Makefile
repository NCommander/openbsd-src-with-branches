#	$OpenBSD$

LIB=	kern

.PATH:	${.CURDIR}/../../../../lib/libkern
.PATH:	${.CURDIR}/..

CFLAGS+=${DEBUGFLAGS}
CFLAGS+=-I${.CURDIR}/..
CFLAGS+=-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern

# kern routines
SRCS=	__main.c ashrdi3.c divdi3.c qdivrem.c strlen.c strcat.c strcmp.c \
	strcpy.c strncmp.c umoddi3.c

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
