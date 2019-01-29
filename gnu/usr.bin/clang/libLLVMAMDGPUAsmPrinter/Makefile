# $OpenBSD$

LIB=	LLVMAMDGPUAsmPrinter
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/InstPrinter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
