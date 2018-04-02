#	$OpenBSD: Makefile.inc,v 1.2 2017/08/27 21:59:52 deraadt Exp $

CFLAGS += -fpic
AFLAGS += -fpic

# extract components from libgcc.a
MDOBJ=	_dyncall.o _remU.o _remI.o _divI.o _divU.o _div_const.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}
