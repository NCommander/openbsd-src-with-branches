# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMPowerPCAsmParser
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/AsmParser
