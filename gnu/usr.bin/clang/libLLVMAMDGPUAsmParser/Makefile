# $OpenBSD$

LIB=	LLVMAMDGPUAsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/AsmParser

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
