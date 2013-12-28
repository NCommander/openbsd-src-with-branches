#	$OpenBSD: Makefile.inc,v 1.1 2013/12/27 14:23:49 jsing Exp $

CPPFLAGS += -I${.CURDIR}/vax
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/vax
SRCS += vax_installboot.c
