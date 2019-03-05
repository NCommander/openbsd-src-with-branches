# $OpenBSD: Makefile,v 1.2 2017/04/17 19:58:53 pascal Exp $

PROG=	llc
NOMAN=

SRCS=	llc.cpp

.PATH: ${.CURDIR}/../../../llvm/tools/llc

LLVM_LIBDEPS=

DPADD+= ${.OBJDIR}/../libLLVM/libLLVM.a
LDADD+= ${.OBJDIR}/../libLLVM/libLLVM.a

.include <bsd.prog.mk>
