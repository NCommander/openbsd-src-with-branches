# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMSparcAsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

SRCS+=	SparcAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/AsmParser
