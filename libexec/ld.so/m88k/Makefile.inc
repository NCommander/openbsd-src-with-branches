#	$OpenBSD: Makefile.inc,v 1.3 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fpic
SRCS+=	memcpy.c

LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_88K_BBASED_32
