# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMMCDisassembler
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	Disassembler.cpp \
	MCDisassembler.cpp \
	MCExternalSymbolizer.cpp \
	MCRelocationInfo.cpp \
	MCSymbolizer.cpp \

.PATH:	${.CURDIR}/../../../llvm/lib/MC/MCDisassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
