# $OpenBSD$

.include <bsd.own.mk>

PROG=	llvm-objdump
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

SRCS=	COFFDump.cpp \
	ELFDump.cpp \
	MachODump.cpp \
	WasmDump.cpp \
	XCOFFDump.cpp \
	llvm-objdump.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objdump

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
