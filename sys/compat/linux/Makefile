#	$OpenBSD: Makefile,v 1.3 2001/08/26 03:23:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1995/02/28 23:26:21 fvdl Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	linux_sysent.c linux_syscalls.c linux_syscall.h linux_syscallargs.h

all:
	@echo "Doing nothing.  Did you want make syscalls?"

syscalls:	linux_sysent.c

${OBJS}: ${DEP}
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
