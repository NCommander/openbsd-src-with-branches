#	$OpenBSD$

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

.if ${ELF_TOOLCHAIN:L} == "yes"
SRCS+=		dlfcn_stubs.c
CPPFLAGS+=	-I${.CURDIR}/dlfcn
.endif
