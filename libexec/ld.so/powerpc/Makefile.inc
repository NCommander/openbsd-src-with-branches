#	$OpenBSD: Makefile.inc,v 1.3 2019/05/10 13:29:21 guenther Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
CFLAGS += -fPIC -msoft-float
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
