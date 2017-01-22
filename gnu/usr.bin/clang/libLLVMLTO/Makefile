# $OpenBSD: Makefile,v 1.2 2017/01/14 20:12:41 patrick Exp $

LIB=	LLVMLTO
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/LTO

.include <bsd.own.mk>
SRCS=	LTO.cpp \
	LTOModule.cpp \
	LTOCodeGenerator.cpp \
	UpdateCompilerUsed.cpp \
	ThinLTOCodeGenerator.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/LTO

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
