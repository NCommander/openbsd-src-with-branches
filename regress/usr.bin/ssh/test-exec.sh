#	$OpenBSD: Makefile,v 1.1 2002/01/17 13:21:28 markus Exp $

PORT=4242
USER=`id -un`
SUDO=
#SUDO=sudo

OBJ=$1
if [ "x$OBJ" = "x" ]; then
	echo '$OBJ not defined'
	exit 2
fi
if [ ! -d $OBJ ]; then
	echo "not a directory: $OBJ"
	exit 2
fi
SCRIPT=$2
if [ "x$SCRIPT" = "x" ]; then
	echo '$SCRIPT not defined'
	exit 2
fi
if [ ! -f $SCRIPT ]; then
	echo "not a file: $SCRIPT"
	exit 2
fi
if sh -n $SCRIPT; then
	true
else
	echo "syntax error in $SCRIPT"
	exit 2
fi
unset SSH_AUTH_SOCK

# helper
cleanup ()
{
	test -f $PIDFILE && $SUDO kill `cat $PIDFILE`
}

trace ()
{
	# echo "$@"
}

fail ()
{
	RESULT=1
	echo "$@"
}

fatal ()
{
	echo -n "FATAL: "
	fail "$@"
	cleanup
	exit $RESULT
}

RESULT=0
PIDFILE=$OBJ/pidfile

trap cleanup 3 2

# create server config
cat << EOF > $OBJ/sshd_config
	Port			$PORT
	ListenAddress		127.0.0.1
	#ListenAddress		::1
	PidFile			$PIDFILE
	AuthorizedKeysFile	$OBJ/authorized_keys_%u
	LogLevel		QUIET
EOF

# server config for proxy connects
cp $OBJ/sshd_config $OBJ/sshd_config_proxy

# create client config
cat << EOF > $OBJ/ssh_config
Host *
	Hostname		127.0.0.1
	HostKeyAlias		localhost-with-alias
	Port			$PORT
	User			$USER
	GlobalKnownHostsFile	$OBJ/known_hosts
	UserKnownHostsFile	$OBJ/known_hosts
	RSAAuthentication	yes
	PubkeyAuthentication	yes
	ChallengeResponseAuthentication	no
	HostbasedAuthentication	no
	KerberosAuthentication	no
	PasswordAuthentication	no
	RhostsAuthentication	no
	RhostsRSAAuthentication	no
EOF

trace "generate keys"
for t in rsa rsa1; do
	# generate user key
	rm -f $OBJ/$t
	ssh-keygen -q -N '' -t $t  -f $OBJ/$t || fail "ssh-keygen for $t failed"

	# known hosts file for client
	(
		echo -n 'localhost-with-alias,127.0.0.1,::1 '
		cat $OBJ/$t.pub
	) >> $OBJ/known_hosts

	# setup authorized keys
	cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
	echo IdentityFile $OBJ/$t >> $OBJ/ssh_config

	# use key as host key, too
	$SUDO cp $OBJ/$t $OBJ/host.$t
	echo HostKey $OBJ/host.$t >> $OBJ/sshd_config

	# don't use SUDO for proxy connect
	echo HostKey $OBJ/$t >> $OBJ/sshd_config_proxy
done

# start sshd
$SUDO sshd -f $OBJ/sshd_config -t	|| fatal "sshd_config broken"
$SUDO sshd -f $OBJ/sshd_config

trace "wait for sshd"
i=0;
while [ ! -f $PIDFILE -a $i -lt 5 ]; do
	i=`expr $i + 1`
	sleep $i
done

test -f $PIDFILE			|| fatal "no sshd running on port $PORT"

# check proxy config
sshd -t -f $OBJ/sshd_config_proxy	|| fail "sshd_config_proxy broken"

# source test body
. $SCRIPT

# kill sshd
cleanup
if [ $RESULT -eq 0 ]; then
	trace ok $tid
else
	echo failed $tid
fi
exit $RESULT
