#	$OpenBSD: Makefile,v 1.1 1996/08/02 20:34:52 niklas Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	hpux_sysent.c hpux_syscalls.c hpux_syscall.h hpux_syscallargs.h

${OBJS}: ${DEP}
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
