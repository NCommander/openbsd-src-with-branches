#	$OpenBSD:$

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "cats") || (${MACHINE} == "hppa")

PROG=	bioctl
.else
NOPROG=
.endif

MAN=	bioctl.8

.include <bsd.prog.mk>
