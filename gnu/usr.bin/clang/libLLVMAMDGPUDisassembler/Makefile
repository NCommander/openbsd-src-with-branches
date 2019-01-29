# $OpenBSD$

LIB=	LLVMAMDGPUDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
