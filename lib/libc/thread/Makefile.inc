#	$OpenBSD: Makefile.inc,v 1.18 2018/10/21 17:07:24 visa Exp $

.PATH: ${LIBCSRCDIR}/thread

SRCS+=	callbacks.c atfork.c

# threads infrastructure
SRCS+=	rthread.c \
	rthread_condattr.c \
	rthread_debug.c \
	rthread_file.c \
	rthread_libc.c \
	rthread_once.c \
	rthread_tls.c \

notyet= rthread_condattr_clock.c \
	rthread_equal.c \
	rthread_exit.c \
	spinlock.c \
	spinlocktry.c

.if ${MACHINE_ARCH} == "hppa" || ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "sh"
SRCS+=	rthread_sync.c
.else
CFLAGS+= -DFUTEX
SRCS+=	rthread_mutex.c \
	rthread_cond.c
.endif

.if defined(NOPIC)
CFLAGS+=-DNO_PIC
.endif

OBJS+=	_atomic_lock.o

