#	$OpenBSD: Makefile.inc,v 1.4 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fpic
AFLAGS += -fpic

# extract components from libgcc.a
MDOBJ=	_dyncall.o _remU.o _remI.o _divI.o _divU.o _div_const.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}
