#	$OpenBSD: Makefile.inc,v 1.2 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fPIC
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_386_RELATIVE
