#	$OpenBSD: Makefile.inc,v 1.2 2013/12/28 11:00:33 jsing Exp $

CPPFLAGS += -I${.CURDIR}/hppa64
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/hppa64
SRCS += hppa64_installboot.c
