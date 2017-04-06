# $OpenBSD$

LIB=	LLVMLinker
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Linker

.include <bsd.own.mk>
SRCS=	IRMover.cpp \
	LinkModules.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Linker

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
