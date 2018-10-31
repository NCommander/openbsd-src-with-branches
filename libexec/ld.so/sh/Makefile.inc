#	$OpenBSD: Makefile.inc,v 1.7 2017/08/27 21:59:52 deraadt Exp $

CFLAGS += -fpic
CFLAGS+=-m4-nofpu
AFLAGS += -D_STANDALONE

# extract components from libgcc.a
MDOBJ=	_divdi3.o _clz.o _udiv_qrnnd_16.o _udivsi3_i4.o _udivdi3.o \
	_sdivsi3.o _udivsi3.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}
