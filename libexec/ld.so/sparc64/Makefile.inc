#	$OpenBSD: Makefile.inc,v 1.1 2002/08/21 15:40:30 art Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
