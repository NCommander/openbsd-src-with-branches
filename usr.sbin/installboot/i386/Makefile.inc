#	$OpenBSD$

CPPFLAGS += -I${.CURDIR}/i386
CFLAGS += -DSOFTRAID

SRCS += softraid.c

.PATH:  ${.CURDIR}/i386
SRCS += i386_installboot.c
SRCS += i386_softraid.c
