#	$OpenBSD: Makefile,v 1.1 1995/10/14 20:24:14 christos Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	hpux_sysent.c hpux_syscalls.c hpux_syscall.h hpux_syscallargs.h

${OBJS}: ${DEP}
	-mv -f hpux_sysent.c hpux_sysent.c.bak
	-mv -f hpux_syscalls.c hpux_syscalls.c.bak
	-mv -f hpux_syscall.h hpux_syscall.h.bak
	-mv -f hpux_syscallargs.h hpux_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
