# $OpenBSD$

LIB=	LLVMIRReader
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/IRReader

.include <bsd.own.mk>
SRCS=	IRReader.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/IRReader

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
