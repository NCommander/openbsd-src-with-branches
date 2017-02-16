#	$OpenBSD: Makefile.inc,v 1.1 2017/01/11 14:11:27 patrick Exp $

CFLAGS += -fpic -fno-stack-protector
CFLAGS += -march=armv8-a+nofp+nosimd # this is -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/aarch64
