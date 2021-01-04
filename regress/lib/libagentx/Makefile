# $OpenBSD: Makefile,v 1.4 2020/10/26 15:57:03 deraadt Exp $

REGRESS_TARGETS= run-regress-agentx

NETSNMP = $((grep _netsnmp /etc/group))

.if empty (NETSNMP)
regress:
	@echo NETSNMP package required.
	@echo SKIPPED
.endif

SRCS=		main.c log.c
PROG=		agentx
CFLAGS=		-Wall
NOMAN=		yes
LDADD=		-lagentx -levent

run-regress-agentx:
	${SUDO} ${.OBJDIR}/agentx

.include <bsd.regress.mk>
