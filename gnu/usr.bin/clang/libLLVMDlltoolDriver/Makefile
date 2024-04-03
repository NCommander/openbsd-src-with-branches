# $OpenBSD$

LIB=	LLVMDlltoolDriver
NOPROFILE=

SRCS+=	DlltoolDriver.cpp

DlltoolDriver.o: Options.inc
Options.inc: Options.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ToolDrivers/llvm-dlltool

CLEANFILES+=	Options.inc

install:
	# Nothing here so far ...

.include <bsd.lib.mk> 
