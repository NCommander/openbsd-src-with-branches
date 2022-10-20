#	$OpenBSD: Makefile,v 1.32 2022/09/12 13:10:03 kn Exp $

SUBDIR += acme-client
SUBDIR += arp
SUBDIR += bgpd
SUBDIR += btrace
SUBDIR += httpd
SUBDIR += ifstated
SUBDIR += installboot
SUBDIR += ldapd
SUBDIR += mtree
SUBDIR += ocspcheck
SUBDIR += ospfd
SUBDIR += ospf6d
SUBDIR += relayd
SUBDIR += rpki-client
SUBDIR += snmpd
SUBDIR += syslogd

.if ${MACHINE} == "amd64"
SUBDIR += vmd
.endif

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>
