#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.17 2000/09/07 04:02:33 rahnds Exp $

.include <bsd.own.mk>

SUBDIR=	atrun comsat fingerd ftpd getNAME getty identd lockspool \
	mail.local makewhatis rexecd rlogind rshd \
	rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd \
	talkd tcpd telnetd tftpd uucpd smtpd
SUBDIR+=login_passwd login_skey login_kerberos login_krb-or-pwd login_reject \
	login_chpass login_lchpass login_token

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif
.if (${MACHINE_ARCH} == "powerpc")
SUBDIR+=ld.so
.endif

.include <bsd.subdir.mk>
