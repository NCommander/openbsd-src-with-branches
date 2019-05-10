#	$OpenBSD: Makefile.inc,v 1.2 2002/12/09 20:56:34 drahn Exp $

CFLAGS += -fpic -msoft-float
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
