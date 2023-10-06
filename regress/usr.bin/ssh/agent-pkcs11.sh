#	$OpenBSD: agent-pkcs11.sh,v 1.9 2021/07/25 12:13:03 dtucker Exp $
#	Placed in the Public Domain.

tid="pkcs11 agent test"

TEST_SSH_PIN=1234
TEST_SSH_SOPIN=12345678
TEST_SSH_PKCS11=/usr/local/lib/softhsm/libsofthsm2.so
if [ "x$TEST_SSH_SSHPKCS11HELPER" != "x" ]; then
	SSH_PKCS11_HELPER="${TEST_SSH_SSHPKCS11HELPER}"
	export SSH_PKCS11_HELPER
fi

test -f "$TEST_SSH_PKCS11" || fatal "$TEST_SSH_PKCS11 does not exist"

# setup environment for softhsm2 token
DIR=$OBJ/SOFTHSM
rm -rf $DIR
TOKEN=$DIR/tokendir
mkdir -p $TOKEN
SOFTHSM2_CONF=$DIR/softhsm2.conf
export SOFTHSM2_CONF
cat > $SOFTHSM2_CONF << EOF
# SoftHSM v2 configuration file
directories.tokendir = ${TOKEN}
objectstore.backend = file
# ERROR, WARNING, INFO, DEBUG
log.level = DEBUG
# If CKF_REMOVABLE_DEVICE flag should be set
slots.removable = false
EOF
out=$(softhsm2-util --init-token --free --label token-slot-0 --pin "$TEST_SSH_PIN" --so-pin "$TEST_SSH_SOPIN")
slot=$(echo -- $out | sed 's/.* //')

# prevent ssh-agent from calling ssh-askpass
SSH_ASKPASS=/usr/bin/true
export SSH_ASKPASS
unset DISPLAY

# start command w/o tty, so ssh-add accepts pin from stdin
# XXX could force askpass instead
notty() {
	perl -e 'use POSIX; POSIX::setsid(); 
	    if (fork) { wait; exit($? >> 8); } else { exec(@ARGV) }' "$@"
}

trace "generating keys"
RSA=${DIR}/RSA
RSAP8=${DIR}/RSAP8
ECPARAM=${DIR}/ECPARAM
EC=${DIR}/EC
ECP8=${DIR}/ECP8
$OPENSSL_BIN genpkey -algorithm rsa > $RSA || fatal "genpkey RSA fail"
$OPENSSL_BIN pkcs8 -nocrypt -in $RSA > $RSAP8 || fatal "pkcs8 RSA fail"
softhsm2-util --slot "$slot" --label 01 --id 01 \
    --pin "$TEST_SSH_PIN" --import $RSAP8 || fatal "softhsm import RSA fail"

$OPENSSL_BIN genpkey \
    -genparam \
    -algorithm ec \
    -pkeyopt ec_paramgen_curve:prime256v1 > $ECPARAM || fatal "param EC fail"
$OPENSSL_BIN genpkey -paramfile $ECPARAM > $EC || fatal "genpkey EC fail"
$OPENSSL_BIN pkcs8 -nocrypt -in $EC > $ECP8 || fatal "pkcs8 EC fail"
softhsm2-util --slot "$slot" --label 02 --id 02 \
    --pin "$TEST_SSH_PIN" --import $ECP8 || fatal "softhasm import EC fail"

trace "start agent"
eval `${SSHAGENT} ${EXTRA_AGENT_ARGS} -s` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	trace "add pkcs11 key to agent"
	echo ${TEST_SSH_PIN} | notty ${SSHADD} -s ${TEST_SSH_PKCS11} > /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-add -s failed: exit code $r"
	fi

	trace "pkcs11 list via agent"
	${SSHADD} -l > /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-add -l failed: exit code $r"
	fi

	for k in $RSA $EC; do
		trace "testing $k"
		chmod 600 $k
		ssh-keygen -y -f $k > $k.pub
		pub=$(cat $k.pub)
		${SSHADD} -L | grep -q "$pub" || \
			fail "key $k missing in ssh-add -L"
		${SSHADD} -T $k.pub || fail "ssh-add -T with $k failed"

		# add to authorized keys
		cat $k.pub > $OBJ/authorized_keys_$USER
		trace "pkcs11 connect via agent ($k)"
		${SSH} -F $OBJ/ssh_proxy somehost exit 5
		r=$?
		if [ $r -ne 5 ]; then
			fail "ssh connect failed (exit code $r)"
		fi
	done

	trace "remove pkcs11 keys"
	echo ${TEST_SSH_PIN} | notty ${SSHADD} -e ${TEST_SSH_PKCS11} > /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-add -e failed: exit code $r"
	fi

	trace "kill agent"
	${SSHAGENT} -k > /dev/null
fi
