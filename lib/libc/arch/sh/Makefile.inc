#	$OpenBSD: Makefile.inc,v 1.1.1.1 2006/10/10 22:07:10 miod Exp $

CPPFLAGS += -DSOFTFLOAT

SOFTFLOAT_BITS=32
.include <arch/arm/softfloat/Makefile.inc>
