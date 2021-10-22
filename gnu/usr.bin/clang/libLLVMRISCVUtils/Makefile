# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMRISCVUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

SRCS+=	RISCVBaseInfo.cpp \
	RISCVMatInt.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/Utils
