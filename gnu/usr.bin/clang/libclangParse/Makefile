# $OpenBSD$

LIB=	clangParse
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	ParseAST.cpp \
	ParseCXXInlineMethods.cpp \
	ParseDecl.cpp \
	ParseDeclCXX.cpp \
	ParseExpr.cpp \
	ParseExprCXX.cpp \
	ParseInit.cpp \
	ParseObjc.cpp \
	ParseOpenMP.cpp \
	ParsePragma.cpp \
	ParseStmt.cpp \
	ParseStmtAsm.cpp \
	ParseTemplate.cpp \
	ParseTentative.cpp \
	Parser.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Parse

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
