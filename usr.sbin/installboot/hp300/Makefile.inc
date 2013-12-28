#	$OpenBSD: Makefile.inc,v 1.2 2013/12/28 11:00:33 jsing Exp $

CPPFLAGS += -I${.CURDIR}/hp300
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/hp300
SRCS += hp300_installboot.c
