#	$OpenBSD: Makefile.inc,v 1.5 2002/08/08 06:00:20 mickey Exp $

KMINCLUDES=	arch/hppa/DEFS.h arch/hppa/SYS.h prefix.h
KMSRCS=

CLEANFILES+=	cerror.S

cerror.S:
	@echo > cerror.S
