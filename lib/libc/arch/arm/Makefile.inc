# $OpenBSD: Makefile.inc,v 1.7 2016/09/17 20:13:48 kettenis Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

CERROR=	cerror.S

CPPFLAGS += -DSOFTFLOAT

# Override softfloat implementations of FP mode control functions
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/gen

SOFTFLOAT_BITS=32
.include <arch/arm/softfloat/Makefile.inc>

.include <arch/arm/dlfcn/Makefile.inc>
