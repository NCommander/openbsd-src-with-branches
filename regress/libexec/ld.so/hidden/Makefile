# $OpenBSD$

.include <bsd.own.mk>

USE_GCC3?=No

.if ${USE_GCC3:L} == "yes"
SUBDIR+= libaa libab test1 test2
.endif

install:

.include <bsd.subdir.mk>
