# $OpenBSD$

LIB=	LLVMBinaryFormat
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

.include <bsd.own.mk>
SRCS=	Dwarf.cpp \
	Magic.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/BinaryFormat

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
