#	$OpenBSD: Makefile,v 1.5 1997/06/23 23:45:17 kstailey Exp $
#	$NetBSD: Makefile,v 1.2 1997/01/22 01:36:30 cgd Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

all:	machine-links

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

.include "${S}/lib/libz/Makefile"
CFLAGS+=${SACFLAGS} -I${S}/lib/libsa
