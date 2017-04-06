# $OpenBSD$

LIB=	LLVMSparcDesc
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	SparcAsmBackend.cpp \
	SparcELFObjectWriter.cpp \
	SparcMCAsmInfo.cpp \
	SparcMCCodeEmitter.cpp \
	SparcMCTargetDesc.cpp \
	SparcMCExpr.cpp \
	SparcTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/MCTargetDesc

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
