#	$OpenBSD: Makefile,v 1.9 2003/11/24 17:12:55 espie Exp $

.include <bsd.own.mk>
USE_GCC3?=No

.if make(obj)
SUBDIR+=libiberty libobjc libstdc++ ../egcs libreadline
.else
.  if ${USE_GCC3:L} == "yes"
SUBDIR+=libiberty libobjc libstdc++ libreadline
.  else
SUBDIR+=libiberty ../egcs libreadline 
.  endif
.endif

.include <bsd.subdir.mk>
