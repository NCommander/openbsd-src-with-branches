#	$OpenBSD: Makefile.inc,v 1.2 2005/12/27 18:42:24 drahn Exp $

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

SRCS+=		dlfcn_stubs.c
