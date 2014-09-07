#	$OpenBSD: Makefile,v 1.1 2004/01/15 21:32:03 millert Exp $

.include <bsd.own.mk>

SUBDIR=	lib sudo visudo
.if !defined(NOPIC)
SUBDIR+= noexec
.endif

.include <bsd.subdir.mk>
