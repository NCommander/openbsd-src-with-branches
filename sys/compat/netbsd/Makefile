#	$OpenBSD: Makefile,v 1.1 1999/09/12 14:15:16 kstailey Exp $

SYS=	../..
DEP=	syscalls.conf syscalls.master ${SYS}/kern/makesyscalls.sh
OBJS=	netbsd_sysent.c netbsd_syscalls.c \
	netbsd_syscall.h netbsd_syscallargs.h

${OBJS}: ${DEP}
	sh ${SYS}/kern/makesyscalls.sh syscalls.conf syscalls.master
