# $OpenBSD$

LIB=	LLVMMCParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC/MCParser

.include <bsd.own.mk>
SRCS=	AsmLexer.cpp \
	AsmParser.cpp \
	COFFAsmParser.cpp \
	DarwinAsmParser.cpp \
	ELFAsmParser.cpp \
	MCAsmLexer.cpp \
	MCAsmParser.cpp \
	MCAsmParserExtension.cpp \
	MCTargetAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/MC/MCParser

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
