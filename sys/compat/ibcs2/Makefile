#	$OpenBSD: Makefile,v 1.1 1995/10/14 20:24:14 christos Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	ibcs2_sysent.c ibcs2_syscalls.c ibcs2_syscall.h ibcs2_syscallargs.h

${OBJS}: ${DEP}
	-mv -f ibcs2_sysent.c ibcs2_sysent.c.bak
	-mv -f ibcs2_syscalls.c ibcs2_syscalls.c.bak
	-mv -f ibcs2_syscall.h ibcs2_syscall.h.bak
	-mv -f ibcs2_syscallargs.h ibcs2_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
