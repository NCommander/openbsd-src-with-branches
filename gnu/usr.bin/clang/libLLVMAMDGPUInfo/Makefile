# $OpenBSD$

LIB=	LLVMAMDGPUInfo
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/TargetInfo

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
