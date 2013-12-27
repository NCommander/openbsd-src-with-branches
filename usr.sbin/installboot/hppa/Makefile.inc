#	$OpenBSD$

CPPFLAGS += -I${.CURDIR}/hppa

SRCS += bootstrap.c

.PATH:  ${.CURDIR}/hppa
SRCS += hppa_installboot.c
