#	$OpenBSD: Makefile,v 1.4.4.4 1996/10/29 10:27:37 mickey Exp $
#	@(#)Makefile	7.9 (Berkeley) 5/8/91

S	= ${.CURDIR}/../../../..
I386	= ${.CURDIR}/..
STAND=	/sys/stand
INCPATH=-I${S}/lib/libsa -I${S} -I${.CURDIR}/../libsa
VPATH=	${STAND}
STANDDIR= ${DESTDIR}/usr/mdec
LDADD=	../libsa/libsa.a
DPADD=	../libsa/libsa.a

DEBUG=	-g -DDEBUG
DEBUG+=	-DARP_DEBUG
DEBUG+=	-DRARP_DEBUG
DEBUG+=	-DNET_DEBUG
DEBUG+=	-DNETIF_DEBUG
DEBUG+=	-DETHER_DEBUG
DEBUG+=	-DEXEC_DEBUG
DEBUG+=	-DBOOTP_DEBUG
DEBUG+=	-DNFS_DEBUG
DEBUG+=	-DRPC_DEBUG

# AFLAGS=	-v -Wa,-a
CFLAGS=	-Wall -m386 -O2 -D_STANDALONE -DAT386 -O ${INCPATH}
CPPFLAGS=	${DEBUG} ${INCPATH}

SRCS=	boot.c conf.c biosdev.c bios.S ${ZLIB} version.c

OBJS=	boot.o zlib.o conf.o
#clock.o

.PATH: ${S}/net
# ${I386}/i386 ${I386}/isa ${S}/dev/isa ${S}/kern

ALL=	boot
all:	config ${ALL}

config:
	@rm -f i386 machine
	@ln -fs ${I386} i386
	@ln -fs ${I386}/include machine

# getting booted from disc

boot:	${OBJS} ${DPADD} version.o
	${LD} -o $@ -e start -N -T 60000 ${OBJS} ${LDADD} version.o
	size $@
	ls -l $@

install: ${ALL}
	install -c ${ALL} ${STANDDIR}

.include <bsd.prog.mk>
