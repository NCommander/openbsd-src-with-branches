#	$OpenBSD: Makefile,v 1.2 1997/01/23 16:22:56 niklas Exp $
#	$NetBSD: Makefile,v 1.1 1996/09/03 03:18:12 mycroft Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	ibcs2_sysent.c ibcs2_syscalls.c ibcs2_syscall.h ibcs2_syscallargs.h

${OBJS}: ${DEP}
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
