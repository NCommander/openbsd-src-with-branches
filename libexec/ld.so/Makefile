#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:55:51 millert Exp $

CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${.CURDIR}/${MACHINE_ARCH}

SRCS=	ldasm.S loader.c library.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
PROG=	ld.so
MAN=	ld.so.8
BINDIR=/usr/libexec
INSTALL_STRIP=

.PATH:	${.CURDIR}/${MACHINE_ARCH}

ELF_LDFLAGS=--shared # using GNU ld

$(PROG):
	$(LD) -x -e _dl_start $(ELF_LDFLAGS) -o $(PROG) $(OBJS)


.include <bsd.prog.mk>
