#	$OpenBSD: Makefile,v 1.4 1997/04/14 11:06:46 deraadt Exp $

PROG=	biosboot
SRCS=	biosboot.S
# AFLAGS+=-Wa,-a
CPPFLAGS+=-DSTART=$(START) -DBOOTMAGIC=$(BOOTMAGIC) -DBOOTREL=$(BOOTREL)
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -N -x -Bstatic
STRIP=
MAN=	biosboot.8

all:	machine-links

machine-links:
	@rm -f i386 machine
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)

.include <bsd.prog.mk>

