# $OpenBSD$

LIB=	LLVMSymbolize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/Symbolize

SRCS+=	DIPrinter.cpp \
	SymbolizableObjectFile.cpp \
	Symbolize.cpp \

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/Symbolize
