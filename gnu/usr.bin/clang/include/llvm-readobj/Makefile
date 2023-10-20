# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../llvm-tblgen/llvm-tblgen
OBJCOPY_INC=${.CURDIR}/../../../../llvm/llvm/tools/llvm-readobj

GEN=	Opts.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

Opts.inc: ${OBJCOPY_INC}/Opts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-readobj \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
