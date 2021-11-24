# $OpenBSD: Makefile,v 1.1 2021/11/23 10:30:08 robert Exp $

PROG=	ar
BINDIR=	/usr/bin

.include <bsd.own.mk>

SRCS=	llvm-ar.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-ar

LLVM_LIBDEPS=	LLVM LLVMDlltoolDriver LLVMLibDriver

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
