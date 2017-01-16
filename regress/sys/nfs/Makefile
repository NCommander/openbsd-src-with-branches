# $OpenBSD: Makefile,v 1.2 2017/01/13 01:33:14 bluhm Exp $

PROGS=		mmap-sysctl-copyin mmap-sysctl-copyout
OBJS=		${PROGS:S/$/.o/}
CLEANFILES=	diskimage

.PHONY: disk nfs mount unconfig clean

disk: unconfig
	dd if=/dev/zero of=diskimage bs=512 count=4k
	vnconfig vnd0 diskimage
	newfs vnd0c

nfs:
	grep '/mnt/regress-nfs-server\>' /etc/exports || \
	    echo /mnt/regress-nfs-server -maproot=0:0 127.0.0.1 >>/etc/exports
	rcctl -f start portmap
	rcctl -f start nfsd
	rcctl -f start mountd

mount: disk nfs
	mkdir -p /mnt/regress-nfs-server
	mount /dev/vnd0c /mnt/regress-nfs-server
	mkdir -p /mnt/regress-nfs-client
	mount -t nfs 127.0.0.1:/mnt/regress-nfs-server /mnt/regress-nfs-client

unconfig:
	-umount -f -t nfs -h 127.0.0.1 -a
	-rmdir /mnt/regress-nfs-client 2>/dev/null || true
	-pkill -KILL mountd || true
	-rcctl -f stop nfsd
	-rcctl -f stop portmap
	-umount -f /dev/vnd0c 2>/dev/null || true
	-rmdir /mnt/regress-nfs-server 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true
	-rm -f stamp-setup

clean: _SUBDIRUSE unconfig
	rm -f a.out [Ee]rrs mklog *.core y.tab.h stamp-* \
	    ${PROGS} ${OBJS} ${_LEXINTM} ${_YACCINTM} ${CLEANFILES}

stamp-setup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} mount
	date >$@

REGRESS_TARGETS+=	run-regress-read
run-regress-read: stamp-setup
	@echo '\n======== $@ ========'
	echo -n $@ >/mnt/regress-nfs-server/read
	[ $@ = "`cat /mnt/regress-nfs-client/read`" ]

REGRESS_TARGETS+=	run-regress-write
run-regress-write: stamp-setup
	@echo '\n======== $@ ========'
	echo -n $@ >/mnt/regress-nfs-client/write
	[ $@ = "`cat /mnt/regress-nfs-server/write`" ]

.for p in ${PROGS}
REGRESS_TARGETS+=	run-regress-${p}
run-regress-${p}: stamp-setup ${p}
	@echo '\n======== $@ ========'
	./${p}
.endfor

REGRESS_TARGETS+=	run-regress-cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

.include <bsd.regress.mk>
