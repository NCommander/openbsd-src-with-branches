#	$OpenBSD: Makefile.inc,v 1.6 2019/10/20 03:44:49 guenther Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
