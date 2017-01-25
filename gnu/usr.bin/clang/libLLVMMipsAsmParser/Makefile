# $OpenBSD: Makefile,v 1.1 2016/11/24 22:32:29 patrick Exp $

LIB=	LLVMMipsAsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips \
		-I${LLVM_SRCS}/lib/Target/Mips

.include <bsd.own.mk>
SRCS=	MipsAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/AsmParser

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
