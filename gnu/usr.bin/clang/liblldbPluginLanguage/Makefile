# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginLanguage
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BlockPointer.cpp \
	CF.cpp \
	CPlusPlusLanguage.cpp \
	CPlusPlusNameParser.cpp \
	ClangHighlighter.cpp \
	Cocoa.cpp \
	CoreMedia.cpp \
	CxxStringTypes.cpp \
	LibCxx.cpp \
	LibCxxAtomic.cpp \
	LibCxxBitset.cpp \
	LibCxxInitializerList.cpp \
	LibCxxList.cpp \
	LibCxxMap.cpp \
	LibCxxOptional.cpp \
	LibCxxQueue.cpp \
	LibCxxTuple.cpp \
	LibCxxUnorderedMap.cpp \
	LibCxxVariant.cpp \
	LibCxxVector.cpp \
	LibStdcpp.cpp \
	LibStdcpp.h \
	LibStdcppTuple.cpp \
	LibStdcppUniquePointer.cpp \
	MSVCUndecoratedNameParser.cpp \
	NSArray.cpp \
	NSDictionary.cpp \
	NSError.cpp \
	NSException.cpp \
	NSIndexPath.cpp \
	NSSet.cpp \
	NSString.cpp \
	ObjCLanguage.cpp \
	ObjCPlusPlusLanguage.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/CPlusPlus
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/ClangCommon
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/ObjC
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Language/ObjCPlusPlus

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
