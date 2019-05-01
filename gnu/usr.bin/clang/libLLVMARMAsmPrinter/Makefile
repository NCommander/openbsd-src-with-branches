# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:34 espie Exp $

LIB=	LLVMARMAsmPrinter
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/InstPrinter
