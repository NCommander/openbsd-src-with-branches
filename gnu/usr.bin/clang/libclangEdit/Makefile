# $OpenBSD$

LIB=	clangEdit
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	Commit.cpp \
	EditedSource.cpp \
	RewriteObjCFoundationAPI.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Edit

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
