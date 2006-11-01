#	$OpenBSD$

DEP=	syscalls.conf syscalls.master ../../../kern/makesyscalls.sh
OBJS=	hpux_sysent.c hpux_syscalls.c hpux_syscall.h hpux_syscallargs.h

${OBJS}: ${DEP}
	sh ../../../kern/makesyscalls.sh syscalls.conf syscalls.master
