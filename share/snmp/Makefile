# $OpenBSD$

FILES=	OPENBSD-SNMPD-CONF.txt OPENBSD-BASE-MIB.txt
FILES+=	OPENBSD-MEM-MIB.txt OPENBSD-SENSORS-MIB.txt

all clean cleandir depend lint obj tags: _SUBDIRUSE

realinstall:
	${INSTALL} -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/snmp/mibs

.include <bsd.prog.mk>
