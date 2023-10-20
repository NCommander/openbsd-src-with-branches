#	$OpenBSD: Makefile,v 1.55 2019/02/13 13:22:14 mpi Exp $

LIB=pthread
LIBCSRCDIR=	${.CURDIR}/../libc

CFLAGS+=-Wall -g -Werror -Wshadow
CFLAGS+=-Werror-implicit-function-declaration
CFLAGS+=-Wsign-compare
CFLAGS+=-I${.CURDIR} -include namespace.h \
	-I${LIBCSRCDIR}/arch/${MACHINE_CPU} -I${LIBCSRCDIR}/include
CDIAGFLAGS=
LDADD = -Wl,-znodelete

.if defined(NOPIC)
CFLAGS+=-DNO_PIC
.endif

VERSION_SCRIPT= ${.CURDIR}/Symbols.map

.PATH: ${.CURDIR}/arch/${MACHINE_CPU}
SRCS=	rthread.c \
	rthread_attr.c \
	rthread_barrier.c \
	rthread_barrier_attr.c \
	rthread_fork.c \
	rthread_getcpuclockid.c \
	rthread_mutex_prio.c \
	rthread_mutexattr.c \
	rthread_np.c \
	rthread_rwlockattr.c \
	rthread_sched.c \
	rthread_stack.c \
	rthread_spin_lock.c \
	sched_prio.c

# Architectures without atomics
.if ${MACHINE_ARCH} == "hppa" || ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "sh"
SRCS+=	rthread_sem_compat.c \
	rthread_rwlock_compat.c
.else
CFLAGS+= -DFUTEX
SRCS+=	rthread_sem.c \
	rthread_rwlock.c
.endif

SRCDIR= ${.CURDIR}/../libpthread
.include "${SRCDIR}/man/Makefile.inc"
.include <bsd.lib.mk>

${OBJS} ${GOBJS} ${POBJS} ${SOBJS} ${DOBJS}: ${.CURDIR}/namespace.h
