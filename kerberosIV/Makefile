#	$OpenBSD: Makefile,v 1.25 1998/09/07 00:47:12 niklas Exp $
#	from @(#)Makefile	5.1 (Berkeley) 6/25/90

SUBDIR=

.ifmake includes
SUBDIR+=include
.endif

SUBDIR+=acl krb kadm kafs kdb

SUBDIR+=ext_srvtab kadmin kadmind kdb_destroy kdb_init kdb_edit \
	kdb_util kdestroy kerberos kinit klist kprop kpropd ksrvtgt \
	ksrvutil kstash kauth kauthd

SUBDIR+=man doc

LIBDIRS=acl krb kadm kafs kdb

.include <bsd.own.mk> # for AFS

.if (${AFS} == "yes")
SUBDIR+=afslog
.endif

build:
	${MAKE} cleandir
	${MAKE} depend
	${SUDO} ${MAKE} includes
	for dir in ${LIBDIRS}; do \
		(cd ${.CURDIR}/$$dir && ${MAKE} && ${SUDO} ${MAKE} install) \
	done

.include <bsd.dep.mk>
.include <bsd.subdir.mk>
