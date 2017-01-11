#	$OpenBSD: Makefile.inc,v 1.4 2012/09/02 16:32:52 jsg Exp $

CFLAGS += -fpic -fno-stack-protector
CFLAGS += -mcpu=cortex-a57+nofp+nosimd # this is -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/aarch64
