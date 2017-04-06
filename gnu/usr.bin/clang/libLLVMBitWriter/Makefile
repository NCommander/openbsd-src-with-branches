# $OpenBSD$

LIB=	LLVMBitWriter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BitWriter.cpp \
	BitcodeWriter.cpp \
	BitcodeWriterPass.cpp \
	ValueEnumerator.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Bitcode/Writer

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
