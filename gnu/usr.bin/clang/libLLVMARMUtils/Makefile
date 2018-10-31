# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMARMUtils
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	ARMBaseInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/Utils

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
