#	$OpenBSD: multiplex.sh,v 1.18 2013/04/06 06:00:22 dtucker Exp $
#	Placed in the Public Domain.

CTL=$OBJ/ctl-sock

tid="connection multiplexing"

DATA=/bin/ls
COPY=$OBJ/ls.copy

wait_for_mux_master_ready()
{
	for i in 1 2 3 4 5; do
		${SSH} -F $OBJ/ssh_config -S $CTL -Ocheck otherhost \
		    >/dev/null 2>&1 && return 0
		sleep $i
	done
	fatal "mux master never becomes ready"
}

start_sshd

start_mux_master()
{
	trace "start master, fork to background"
	${SSH} -Nn2 -MS$CTL -F $OBJ/ssh_config -oSendEnv="_XXX_TEST" somehost \
	    -E $TEST_REGRESS_LOGFILE 2>&1 &
	MASTER_PID=$!
	wait_for_mux_master_ready
}

start_mux_master

verbose "test $tid: envpass"
trace "env passing over multiplexed connection"
_XXX_TEST=blah ${SSH} -F $OBJ/ssh_config -oSendEnv="_XXX_TEST" -S$CTL otherhost sh << 'EOF'
	test X"$_XXX_TEST" = X"blah"
EOF
if [ $? -ne 0 ]; then
	fail "environment not found"
fi

verbose "test $tid: transfer"
rm -f ${COPY}
trace "ssh transfer over multiplexed connection and check result"
${SSH} -F $OBJ/ssh_config -S$CTL otherhost cat ${DATA} > ${COPY}
test -f ${COPY}			|| fail "ssh -Sctl: failed copy ${DATA}" 
cmp ${DATA} ${COPY}		|| fail "ssh -Sctl: corrupted copy of ${DATA}"

rm -f ${COPY}
trace "ssh transfer over multiplexed connection and check result"
${SSH} -F $OBJ/ssh_config -S $CTL otherhost cat ${DATA} > ${COPY}
test -f ${COPY}			|| fail "ssh -S ctl: failed copy ${DATA}" 
cmp ${DATA} ${COPY}		|| fail "ssh -S ctl: corrupted copy of ${DATA}"

rm -f ${COPY}
trace "sftp transfer over multiplexed connection and check result"
echo "get ${DATA} ${COPY}" | \
	${SFTP} -S ${SSH} -F $OBJ/ssh_config -oControlPath=$CTL otherhost >>$TEST_REGRESS_LOGFILE 2>&1
test -f ${COPY}			|| fail "sftp: failed copy ${DATA}" 
cmp ${DATA} ${COPY}		|| fail "sftp: corrupted copy of ${DATA}"

rm -f ${COPY}
trace "scp transfer over multiplexed connection and check result"
${SCP} -S ${SSH} -F $OBJ/ssh_config -oControlPath=$CTL otherhost:${DATA} ${COPY} >>$TEST_REGRESS_LOGFILE 2>&1
test -f ${COPY}			|| fail "scp: failed copy ${DATA}" 
cmp ${DATA} ${COPY}		|| fail "scp: corrupted copy of ${DATA}"

rm -f ${COPY}

for s in 0 1 4 5 44; do
	trace "exit status $s over multiplexed connection"
	verbose "test $tid: status $s"
	${SSH} -F $OBJ/ssh_config -S $CTL otherhost exit $s
	r=$?
	if [ $r -ne $s ]; then
		fail "exit code mismatch for protocol $p: $r != $s"
	fi

	# same with early close of stdout/err
	trace "exit status $s with early close over multiplexed connection"
	${SSH} -F $OBJ/ssh_config -S $CTL -n otherhost \
                exec sh -c \'"sleep 2; exec > /dev/null 2>&1; sleep 3; exit $s"\'
	r=$?
	if [ $r -ne $s ]; then
		fail "exit code (with sleep) mismatch for protocol $p: $r != $s"
	fi
done

verbose "test $tid: cmd check"
${SSH} -F $OBJ/ssh_config -S $CTL -Ocheck otherhost >>$TEST_REGRESS_LOGFILE 2>&1 \
    || fail "check command failed" 

verbose "test $tid: cmd exit"
${SSH} -F $OBJ/ssh_config -S $CTL -Oexit otherhost >>$TEST_REGRESS_LOGFILE 2>&1 \
    || fail "send exit command failed" 

# Wait for master to exit
wait $MASTER_PID
kill -0 $MASTER_PID >/dev/null 2>&1 && fail "exit command failed"

# Restart master and test -O stop command with master using -N
verbose "test $tid: cmd stop"
trace "restart master, fork to background"
start_mux_master

# start a long-running command then immediately request a stop
${SSH} -F $OBJ/ssh_config -S $CTL otherhost "sleep 10; exit 0" \
     >>$TEST_REGRESS_LOGFILE 2>&1 &
SLEEP_PID=$!
${SSH} -F $OBJ/ssh_config -S $CTL -Ostop otherhost >>$TEST_REGRESS_LOGFILE 2>&1 \
    || fail "send stop command failed"

# wait until both long-running command and master have exited.
wait $SLEEP_PID
[ $! != 0 ] || fail "waiting for concurrent command"
wait $MASTER_PID
[ $! != 0 ] || fail "waiting for master stop"
kill -0 $MASTER_PID >/dev/null 2>&1 && fail "stop command failed"
