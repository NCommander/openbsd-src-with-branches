#	$OpenBSD: exit-status.sh,v 1.7 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="remote exit status"

for s in 0 1 4 5 44; do
	trace "status $s"
	verbose "test $tid: status $s"
	${SSH} -F $OBJ/ssh_proxy otherhost exit $s
	r=$?
	if [ $r -ne $s ]; then
		fail "exit code mismatch for: $r != $s"
	fi

	# same with early close of stdout/err
	${SSH} -F $OBJ/ssh_proxy -n otherhost exec \
	    sh -c \'"sleep 2; exec > /dev/null 2>&1; sleep 3; exit $s"\'
	r=$?
	if [ $r -ne $s ]; then
		fail "exit code (with sleep) mismatch for: $r != $s"
	fi
done
