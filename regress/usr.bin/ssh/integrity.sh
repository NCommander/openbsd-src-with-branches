#	$OpenBSD: integrity.sh,v 1.10 2013/05/17 01:32:11 dtucker Exp $
#	Placed in the Public Domain.

tid="integrity"

# start at byte 2900 (i.e. after kex) and corrupt at different offsets
# XXX the test hangs if we modify the low bytes of the packet length
# XXX and ssh tries to read...
tries=10
startoffset=2900
macs=`${SSH} -Q mac`
# The following are not MACs, but ciphers with integrated integrity. They are
# handled specially below.
macs="$macs `${SSH} -Q cipher | grep gcm@openssh.com`"

# sshd-command for proxy (see test-exec.sh)
cmd="sh ${SRC}/sshd-log-wrapper.sh ${SSHD} ${TEST_SSHD_LOGFILE} -i -f $OBJ/sshd_proxy"

for m in $macs; do
	trace "test $tid: mac $m"
	elen=0
	epad=0
	emac=0
	ecnt=0
	skip=0
	for off in $(jot $tries $startoffset); do
		if [ $((skip--)) -gt 0 ]; then
			# avoid modifying the high bytes of the length
			continue
		fi
		# modify output from sshd at offset $off
		pxy="proxycommand=$cmd | $OBJ/modpipe -wm xor:$off:1"
		case $m in
			aes*gcm*)	macopt="-c $m";;
			*)		macopt="-m $m";;
		esac
		verbose "test $tid: $m @$off"
		${SSH} $macopt -2F $OBJ/ssh_proxy -o "$pxy" \
		    999.999.999.999 'printf "%4096s" " "' >/dev/null
		if [ $? -eq 0 ]; then
			fail "ssh -m $m succeeds with bit-flip at $off"
		fi
		ecnt=$((ecnt+1))
		output=$(tail -2 $TEST_SSH_LOGFILE | egrep -v "^debug" | \
		     tr -s '\r\n' '.')
		case "$output" in
		Bad?packet*)	elen=$((elen+1)); skip=2;;
		Corrupted?MAC* | Decryption?integrity?check?failed*)
				emac=$((emac+1)); skip=0;;
		padding*)	epad=$((epad+1)); skip=0;;
		*)		fail "unexpected error mac $m at $off";;
		esac
	done
	verbose "test $tid: $ecnt errors: mac $emac padding $epad length $elen"
	if [ $emac -eq 0 ]; then
		fail "$m: no mac errors"
	fi
	expect=$((ecnt-epad-elen))
	if [ $emac -ne $expect ]; then
		fail "$m: expected $expect mac errors, got $emac"
	fi
done
