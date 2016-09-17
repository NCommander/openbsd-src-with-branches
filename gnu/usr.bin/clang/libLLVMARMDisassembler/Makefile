# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	LLVMARMDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	ARMDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/Disassembler

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
