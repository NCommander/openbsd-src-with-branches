#	$OpenBSD: Makefile.inc,v 1.6 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fpic
CFLAGS += -march=rv64gc		# this prevents the use of float in ld.so
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/riscv64
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_RISCV_RELATIVE
