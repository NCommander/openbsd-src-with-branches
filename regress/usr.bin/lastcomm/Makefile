# $OpenBSD: Makefile,v 1.5 2019/09/10 19:01:24 bluhm Exp $

# Start with a clean /var/account/acct accounting file and turn on
# process accounting with accton(8).  Each test executes a command
# with a unique name and checks the flags in the lastcomm(1) output.
# Run tests with fork, map, core, xsig, pledge, trap accounting.

.if ! (make(clean) || make(cleandir) || make(obj))

MOUNT_OBJ !!= mount | grep ^$$(df -P . | tail -1 | awk '{ print $$1 }')

.if "${MOUNT_OBJ:M*wxallowed*}" == ""
REGRESS_SKIP_TARGETS +=	run-syscallwx
.endif

.endif

.if ${MACHINE} == amd64 || ${MACHINE} == armv7
# calling trapsignal() from syscall path does not work on amd64
REGRESS_EXPECTED_FAILURES +=	run-callstack run-syscallwx
.endif

PROGS=			crash trapstack callstack syscallwx
WARNINGS=		Yes
LDADD_syscallwx=	-z wxneeded
CLEANFILES=		regress-*

REGRESS_SETUP_ONCE =	setup-rotate
# Rotate accouting files and keep statistics, from /etc/daily.
setup-rotate:
	@echo '\n======== $@ ========'
	${SUDO} touch /var/account/acct
	-${SUDO} mv -f /var/account/acct.2 /var/account/acct.3
	-${SUDO} mv -f /var/account/acct.1 /var/account/acct.2
	-${SUDO} mv -f /var/account/acct.0 /var/account/acct.1
	${SUDO} cp -f /var/account/acct /var/account/acct.0
	${SUDO} sa -sq
	${SUDO} accton /var/account/acct

REGRESS_TARGETS +=	run-fork
run-fork:
	@echo '\n======== $@ ========'
	# Create shell program, fork a sub shell, and check the -F flag.
	cp -f /bin/sh regress-fork
	./regress-fork -c '( : ) &'
	lastcomm regress-fork | grep -q ' -F '

REGRESS_TARGETS +=	run-trapstack
run-trapstack: trapstack
	@echo '\n======== $@ ========'
	# Use invalid stack pointer, trap, SIGSEGV handler, check -M flag.
	cp -f trapstack regress-trapstack
	./regress-trapstack
	lastcomm regress-trapstack | grep -q ' -MT '

REGRESS_TARGETS +=	run-callstack
run-callstack: callstack
	@echo '\n======== $@ ========'
	# Use invalid stack pointer, syscall, SIGSEGV handler, check -M flag.
	cp -f callstack regress-callstack
	./regress-callstack
	lastcomm regress-callstack | grep -q ' -MT '

REGRESS_TARGETS +=	run-syscallwx
run-syscallwx: syscallwx
	@echo '\n======== $@ ========'
	# Use writable syscall code, run SIGSEGV handler, check -M flag.
	cp -f syscallwx regress-syscallwx
	./regress-syscallwx
	lastcomm regress-syscallwx | grep -q ' -MT '

REGRESS_TARGETS +=	run-core
run-core:
	@echo '\n======== $@ ========'
	# Create shell program, abort sub shell, and check the -DX flag.
	cp -f /bin/sh regress-core
	rm -f regress-core.core
	ulimit -c 100000; ./regress-core -c '( : ) & kill -SEGV $$!; wait'
	lastcomm regress-core | grep -q ' -FDX '

REGRESS_TARGETS +=	run-xsig
run-xsig:
	@echo '\n======== $@ ========'
	# Create shell program, kill sub shell, and check the -X flag.
	cp -f /bin/sh regress-xsig
	./regress-xsig -c '( : ) & kill -KILL $$!; wait'
	lastcomm regress-xsig | grep -q ' -FX '

REGRESS_TARGETS +=	run-pledge
run-pledge:
	@echo '\n======== $@ ========'
	# Create perl program, violate pledge, and check the -P flag.
	cp -f /usr/bin/perl regress-pledge
	ulimit -c 0; ! ./regress-pledge -MOpenBSD::Pledge -e\
	    'pledge("stdio") or die $$!; chdir("/")'
	lastcomm regress-pledge | grep -q ' -XP '

REGRESS_TARGETS +=	run-trap
run-trap: crash
	@echo '\n======== $@ ========'
	# Build crashing program, run SIGSEGV handler, and check the -T flag.
	cp -f crash regress-trap
	./regress-trap
	lastcomm regress-trap | grep -q ' -T '

${REGRESS_TARGETS}: ${PROGS}

.include <bsd.regress.mk>
