# $OpenBSD$

LIB=	clangSupport
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS+=	RISCVVIntrinsicUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Support

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
