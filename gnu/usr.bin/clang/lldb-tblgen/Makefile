# $OpenBSD: Makefile,v 1.10 2019/06/23 22:07:45 patrick Exp $

PROG=	lldb-tblgen
NOMAN=

.include <bsd.own.mk>
SRCS=	LLDBOptionDefEmitter.cpp \
	LLDBPropertyDefEmitter.cpp \
	LLDBTableGen.cpp \
	LLDBTableGenUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/utils/TableGen

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
