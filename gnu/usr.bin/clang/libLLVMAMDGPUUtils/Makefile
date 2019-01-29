# $OpenBSD$

LIB=	LLVMAMDGPUUtils
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUBaseInfo.cpp \
	AMDKernelCodeTUtils.cpp \
	AMDGPUAsmUtils.cpp \
	AMDGPULaneDominator.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/Utils

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
