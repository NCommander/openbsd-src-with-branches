# $OpenBSD: Makefile,v 1.2 2017/01/14 20:12:41 patrick Exp $

LIB=	LLVMPasses
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes

.include <bsd.own.mk>
SRCS=	PassBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Passes

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
