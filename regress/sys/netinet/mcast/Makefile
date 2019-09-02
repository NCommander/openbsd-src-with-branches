# $OpenBSD$

PROGS =			mcsend mcrecv mcroute
WARNINGS =		Yes
CLEANFILES =		stamp-* *.log
MSG !!=			echo $$RANDOM

REGRESS_SETUP_ONCE =	setup-sudo
setup-sudo:
	${SUDO} true
.if ! empty(REMOTE_SSH)
	ssh -t ${REMOTE_SSH} ${SUDO} true
.endif
.if ! empty(TARGET_SSH)
	ssh -t ${TARGET_SSH} ${SUDO} true
.endif

REGRESS_TARGETS +=	run-localhost
run-localhost:
	@echo '\n======== $@ ========'
	# send over localhost interface
	./mcrecv -f recv.log -i 127.0.0.1 -r 5 -- \
	./mcsend -f send.log -i 127.0.0.1 -m '${MSG}'
	grep '> ${MSG}$$' send.log
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-localhost-loop
run-localhost-loop:
	@echo '\n======== $@ ========'
	# explicitly enable loop back on multicast interface
	./mcrecv -f recv.log -i 127.0.0.1 -r 5 -- \
	./mcsend -f send.log -i 127.0.0.1 -l 1 -m '${MSG}'
	grep '> ${MSG}$$' send.log
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-localhost-loop0
run-localhost-loop0:
	@echo '\n======== $@ ========'
	# disable loop back on multicast interface, must fail
	./mcrecv -f recv.log -i 127.0.0.1 -n 1 -- \
	./mcsend -f send.log -i 127.0.0.1 -l 0 -m '${MSG}'
	grep '> ${MSG}$$' send.log
	! grep '< ' recv.log

REGRESS_TARGETS +=	run-localhost-ttl0
run-localhost-ttl0:
	@echo '\n======== $@ ========'
	# send over localhost interface
	./mcrecv -f recv.log -i 127.0.0.1 -r 5 -- \
	./mcsend -f send.log -i 127.0.0.1 -m '${MSG}' -t 0
	grep '> ${MSG}$$' send.log
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-localaddr
run-localaddr:
	@echo '\n======== $@ ========'
	# send over a local physical interface
	./mcrecv -f recv.log -i ${LOCAL_ADDR} -r 5 -- \
	./mcsend -f send.log -i ${LOCAL_ADDR} -m '${MSG}'
	grep '> ${MSG}$$' send.log
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-localaddr-loop0
run-localaddr-loop0:
	@echo '\n======== $@ ========'
	# send over physical interface to loopback, ttl is 0
	./mcrecv -f recv.log -i ${LOCAL_ADDR} -n 1 -- \
	./mcsend -f send.log -i ${LOCAL_ADDR} -l 0 -m '${MSG}'
	grep '> ${MSG}$$' send.log
	! grep '< ' recv.log

REGRESS_TARGETS +=	run-localaddr-ttl0
run-localaddr-ttl0:
	@echo '\n======== $@ ========'
	# send over physical interface to loopback, ttl is 0
	./mcrecv -f recv.log -i ${LOCAL_ADDR} -r 5 -- \
	./mcsend -f send.log -i ${LOCAL_ADDR} -m '${MSG}' -t 0
	grep '> ${MSG}$$' send.log
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-remoteaddr
run-remoteaddr:
	@echo '\n======== $@ ========'
	# send over a local physical interface
	./mcrecv -f recv.log -i ${LOCAL_ADDR} -r 5 -- \
	ssh ${REMOTE_SSH} ${.OBJDIR}/mcsend \
	    -f send.log -i ${REMOTE_ADDR} -m '${MSG}'
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-remoteaddr-loop0
run-remoteaddr-loop0:
	@echo '\n======== $@ ========'
	# send over a local physical interface
	./mcrecv -f recv.log -i ${LOCAL_ADDR} -r 5 -- \
	ssh ${REMOTE_SSH} ${.OBJDIR}/mcsend \
	    -f send.log -i ${REMOTE_ADDR} -l 0 -m '${MSG}'
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-remoteaddr-ttl0
run-remoteaddr-ttl0:
	@echo '\n======== $@ ========'
	# send over a local physical interface
	./mcrecv -f recv.log -i ${LOCAL_ADDR} -n 2 -- \
	ssh ${REMOTE_SSH} ${.OBJDIR}/mcsend -f send.log \
	    -i ${REMOTE_ADDR} -m '${MSG}' -t 0
	! grep '< ' recv.log

REGRESS_TARGETS +=	run-forward
run-forward:
	@echo '\n======== $@ ========'
	# start multicast router, start receiver, start sender
	ssh ${REMOTE_SSH} ${SUDO} pkill mcroute || true
	ssh ${REMOTE_SSH} ${SUDO} ${.OBJDIR}/mcroute -b -f route.log \
	    -g 224.0.1.123 -i ${OTHER_ADDR} -o ${REMOTE_ADDR} -r 5
.if empty(TARGET_SSH)
	./mcrecv -f recv.log -g 224.0.1.123 -i ${LOCAL_ADDR} -r 5 -- \
	./mcsend -f send.log \
	    -g 224.0.1.123 -i ${TARGET_ADDR} -l 0 -m '${MSG}' -t 2
	grep '> ${MSG}$$' send.log
.else
	./mcrecv -f recv.log -g 224.0.1.123 -i ${LOCAL_ADDR} -r 5 -- \
	ssh ${TARGET_SSH} ${.OBJDIR}/mcsend -f send.log \
	    -g 224.0.1.123 -i ${TARGET_ADDR} -l 0 -m '${MSG}' -t 2
.endif
	grep '< ${MSG}$$' recv.log

REGRESS_TARGETS +=	run-forward-ttl1
run-forward-ttl1:
	@echo '\n======== $@ ========'
	# try to get ttl 1 over multicast router, must fail
	ssh ${REMOTE_SSH} ${SUDO} pkill mcroute || true
	ssh ${REMOTE_SSH} ${SUDO} ${.OBJDIR}/mcroute -b -f route.log \
	    -g 224.0.1.123 -i ${OTHER_ADDR} -o ${REMOTE_ADDR} -n 3
.if empty(TARGET_SSH)
	./mcrecv -f recv.log -g 224.0.1.123 -i ${LOCAL_ADDR} -n 2 -- \
	./mcsend -f send.log \
	    -g 224.0.1.123 -i ${TARGET_ADDR} -l 0 -m '${MSG}' -t 1
	grep '> ${MSG}$$' send.log
.else
	./mcrecv -f recv.log -g 224.0.1.123 -i ${LOCAL_ADDR} -n 2 -- \
	ssh ${TARGET_SSH} ${.OBJDIR}/mcsend -f send.log \
	    -g 224.0.1.123 -i ${TARGET_ADDR} -l 0 -m '${MSG}' -t 1
.endif
	! grep '< ' recv.log

REGRESS_TARGETS +=	run-forward-local
run-forward-local:
	@echo '\n======== $@ ========'
	# try to get local multicast group over router, must fail
	ssh ${REMOTE_SSH} ${SUDO} pkill mcroute || true
	ssh ${REMOTE_SSH} ${SUDO} ${.OBJDIR}/mcroute -b -f route.log \
	    -g 224.0.0.123 -i ${OTHER_ADDR} -o ${REMOTE_ADDR} -n 3
.if empty(TARGET_SSH)
	./mcrecv -f recv.log -g 224.0.0.123 -i ${LOCAL_ADDR} -n 2 -- \
	./mcsend -f send.log \
	    -g 224.0.0.123 -i ${TARGET_ADDR} -l 0 -m '${MSG}' -t 2
	grep '> ${MSG}$$' send.log
.else
	./mcrecv -f recv.log -g 224.0.0.123 -i ${LOCAL_ADDR} -n 2 -- \
	ssh ${TARGET_SSH} ${.OBJDIR}/mcsend -f send.log \
	    -g 224.0.0.123 -i ${TARGET_ADDR} -l 0 -m '${MSG}' -t 2
.endif
	! grep '< ' recv.log

stamp-remote-build:
	ssh ${REMOTE_SSH} ${MAKE} -C ${.CURDIR} ${PROGS}
	date >$@

stamp-target-build:
	ssh ${TARGET_SSH} ${MAKE} -C ${.CURDIR} ${PROGS}
	date >$@

${REGRESS_TARGETS}: ${PROGS}
${REGRESS_TARGETS:M*-remoteaddr*}: stamp-remote-build
${REGRESS_TARGETS:M*-forward*}: stamp-remote-build
.if ! empty(TARGET_SSH)
${REGRESS_TARGETS:M*-forward*}: stamp-target-build
.endif

.if empty(LOCAL_ADDR)
REGRESS_SKIP_TARGETS +=	${REGRESS_TARGETS:M*-localaddr*}
.endif
.if empty(REMOTE_ADDR) || empty(REMOTE_SSH)
REGRESS_SKIP_TARGETS +=	${REGRESS_TARGETS:M*-remoteaddr*}
REGRESS_SKIP_TARGETS +=	${REGRESS_TARGETS:M*-forward*}
.endif

check-setup:
	! ssh ${REMOTE_SSH} route -n get 224/4
	ssh ${REMOTE_SSH} sysctl net.inet.ip.mforwarding | fgrep =1

.include <bsd.regress.mk>

stamp-remote-build: ${SRCS}
stamp-target-build: ${SRCS}
