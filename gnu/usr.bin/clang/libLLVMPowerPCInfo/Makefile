# $OpenBSD$

LIB=	LLVMPowerPCInfo
NOPIC=
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

.include <bsd.own.mk>
SRCS=	PowerPCTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/TargetInfo

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
