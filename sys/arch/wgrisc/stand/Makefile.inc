#	$OpenBSD: Makefile.inc,v 1.1 1997/05/11 16:17:48 pefo Exp $

CPPFLAGS+=-Wall -Werror -I. -Imachine
CPPFLAGS+=-D_STANDALONE -DSTRIPPED -DALLOC_FIRST_FIT -DNO_SYM_LINKS
CFLAGS+=-O2 -mno-abicalls -mips1 -mcpu=r3000
BOOTREL=0x3000
BOOTMAGIC=0xdeadbeef

.if exists(${SADIR}/libsa/obj)
LIBSA=${SADIR}/libsa/obj/libsa.a
.else
LIBSA=${SADIR}/libsa/libsa.a
.endif
.if exists(${SADIR}/libz/obj)
LIBZ=${SADIR}/libz/obj/libz.a
.else
LIBZ=${SADIR}/libz/libz.a
.endif

BINDIR= /usr/mdec

