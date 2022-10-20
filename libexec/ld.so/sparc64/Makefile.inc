#	$OpenBSD: Makefile.inc,v 1.3 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

.if ${COMPILER_VERSION} == "clang"
# extract components from libcompiler_rt.a
MDOBJ=	multi3.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}
.endif

RELATIVE_RELOC=R_SPARC_RELATIVE
