#	$OpenBSD$
#	@(#)Makefile	7.9 (Berkeley) 5/8/91

S	= ${.CURDIR}/../../..
SAREL	=
I386	= ${.CURDIR}/..
STAND=	/sys/stand
INCPATH=-I. -I${SADIR} -I${S}
VPATH=	${STAND}
STANDDIR= ${DESTDIR}/usr/mdec

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
CPPFLAGS=	${DEBUG} ${INCPATH} -DRELOC=0x${RELOC}

.include "${S}/lib/libsa/Makefile.inc"
LDADD=		${SALIB}
DPADD=		${SALIB}

SRCS=	srt0.S boot.c fs.c dev.c biosdev.c bios.S ${ZLIB} version.c

CLEANFILES+=	*.o *.h bread[fwsc]d.c \
		[fwsc]dboot boot[fwsc]d boot[fwsc]d.sym boot[fwsc]d.c

OBJS=	srt0.o boot.o zlib.o
OBJS+=	fs.o dev.o biosdev.o bios.o
#clock.o
OBJS+=	machdep.o real_prot.o

.PATH: ${S}/net
# ${I386}/i386 ${I386}/isa ${S}/dev/isa ${S}/kern

ALL=	boot
all:	config ${ALL}

config:
	@rm -f i386 machine
	@ln -fs ${I386} i386
	@ln -fs ${I386}/include machine

# startups

srt0.o: ${.CURDIR}/srt0.S
	${CC} -c ${CPPFLAGS} ${AFLAGS} -o ${.TARGET} ${.CURDIR}/srt0.S

# getting booted from disc

boot:	${OBJS} ${DPADD} version.o
	${LD} -o $@ -N -T 0x90000 ${OBJS} ${LDADD} version.o
	size $@
	ls -l $@

install: ${ALL}
	install -c ${ALL} ${STANDDIR}

make.out::
	rm -f ${OBJS}
	${MAKE} ${.MAKEFLAGS} ${OBJS} > make.out

clean::

.include <bsd.prog.mk>

.MAKE:  make.out                      
