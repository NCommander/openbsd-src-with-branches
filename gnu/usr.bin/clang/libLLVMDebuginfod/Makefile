# $OpenBSD$

LIB=	LLVMDebuginfod
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Debuginfod

SRCS+=	BuildIDFetcher.cpp \
	Debuginfod.cpp \
	HTTPClient.cpp \
	HTTPServer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Debuginfod
