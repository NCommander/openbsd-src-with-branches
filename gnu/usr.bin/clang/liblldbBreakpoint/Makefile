# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldbBreakpoint
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	Breakpoint.cpp \
	BreakpointID.cpp \
	BreakpointIDList.cpp \
	BreakpointList.cpp \
	BreakpointLocation.cpp \
	BreakpointLocationCollection.cpp \
	BreakpointLocationList.cpp \
	BreakpointName.cpp \
	BreakpointOptions.cpp \
	BreakpointResolver.cpp \
	BreakpointResolverAddress.cpp \
	BreakpointResolverFileLine.cpp \
	BreakpointResolverFileRegex.cpp \
	BreakpointResolverName.cpp \
	BreakpointSite.cpp \
	BreakpointSiteList.cpp \
	Stoppoint.cpp \
	StoppointCallbackContext.cpp \
	StoppointLocation.cpp \
	Watchpoint.cpp \
	WatchpointList.cpp \
	WatchpointOptions.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Breakpoint

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
