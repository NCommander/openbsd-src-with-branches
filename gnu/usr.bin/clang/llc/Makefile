# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:42 jsg Exp $

.include <bsd.own.mk>

PROG=	llc
NOMAN=

SRCS=	llc.cpp

.PATH: ${.CURDIR}/../../../llvm/tools/llc

LLVM_LIBDEPS=	LLVM

.include <bsd.prog.mk>
