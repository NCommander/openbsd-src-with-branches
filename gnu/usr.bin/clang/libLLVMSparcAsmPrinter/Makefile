# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMSparcAsmPrinter
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

SRCS+=	SparcInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/InstPrinter
