#	$OpenBSD: Makefile,v 1.21 2003/04/17 03:43:18 drahn Exp $

MAN=	biosboot.8

.if ${MACHINE} == "amd64"
PROG=	biosboot
SRCS=	biosboot.S
LD=ld
LDFLAGS=-melf_i386 -nostdlib -Ttext 0 -N -x -Bstatic
INSTALL_STRIP=
SADIR=	${.CURDIR}/..

${PROG}: $(OBJS) $(DPADD)
	@rm -f $(PROG)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	#@size $(PROG)

CPPFLAGS+=-DLOADADDR=$(LOADADDR) -DLINKADDR=$(LINKADDR) -DBOOTMAGIC=$(BOOTMAGIC)
CPPFLAGS+=${DEBUGFLAGS}
AFLAGS+=-m32 # -Wa,-a
.else
NOPROG=
.endif

.include <bsd.prog.mk>

