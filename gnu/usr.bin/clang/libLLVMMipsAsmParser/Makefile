# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMMipsAsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips \
		-I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/AsmParser
