#	$OpenBSD$

CPPFLAGS += -I${.CURDIR}/sparc64
CFLAGS += -DSOFTRAID

.PATH:  ${.CURDIR}/sparc64
SRCS += sparc64_installboot.c
SRCS += sparc64_softraid.c
