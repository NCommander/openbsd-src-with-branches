#	$OpenBSD: Makefile,v 1.15 2010/05/25 00:24:58 espie Exp $

.include <bsd.own.mk>

SUBDIR+=libiberty libreadline
.if make(obj)
SUBDIR+=libobjc libstdc++ ../egcs \
libstdc++-v3 libsupc++-v3 ../usr.bin/cc/libobjc
.else
.  if ${COMPILER_VERSION:L} == "gcc3"
SUBDIR+=libobjc libstdc++
.  elif ${COMPILER_VERSION:L} == "gcc4"
# XXX make sure we build libobjc & libstdc++-v3 from gcc4
SUBDIR+=../usr.bin/cc/libobjc
SUBDIR+=libstdc++-v3 libsupc++-v3
.  elif ${COMPILER_VERSION:L} == "gcc2"
SUBDIR+=libobjc ../egcs
.  endif
.endif

.include <bsd.subdir.mk>
