# $OpenBSD$

LIB=	LLVMIRPrinter
NOPROFILE=

SRCS+=	IRPrinterIRPrintingPasses.cpp

IRPrinterIRPrintingPasses.cpp:
	ln -s ${LLVM_SRCS}/lib/IRPrinter/IRPrintingPasses.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/IRPrinter
