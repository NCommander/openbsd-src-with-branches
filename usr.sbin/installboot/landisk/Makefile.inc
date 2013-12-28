#	$OpenBSD: Makefile.inc,v 1.2 2013/12/28 11:00:33 jsing Exp $

CPPFLAGS += -I${.CURDIR}/landisk
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/landisk
SRCS += landisk_installboot.c
