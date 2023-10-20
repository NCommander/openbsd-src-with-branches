#	$OpenBSD: test-exec.sh,v 1.100 2023/10/20 06:56:45 dtucker Exp $
#	Placed in the Public Domain.

#SUDO=sudo

if [ ! -z "$TEST_SSH_ELAPSED_TIMES" ]; then
	STARTTIME=`date '+%s'`
fi

if [ ! -z "$TEST_SSH_PORT" ]; then
	PORT="$TEST_SSH_PORT"
else
	PORT=4242
fi

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

USER=`id -un`

SRC=`dirname ${SCRIPT}`

# defaults
SSH=ssh
SSHD=sshd
SSHAGENT=ssh-agent
SSHADD=ssh-add
SSHKEYGEN=ssh-keygen
SSHKEYSCAN=ssh-keyscan
SFTP=sftp
SFTPSERVER=/usr/libexec/sftp-server
SCP=scp

# Interop testing
PLINK=/usr/local/bin/plink
PUTTYGEN=/usr/local/bin/puttygen
CONCH=/usr/local/bin/conch
DROPBEAR=/usr/local/bin/dropbear
DBCLIENT=/usr/local/bin/dbclient
DROPBEARKEY=/usr/local/bin/dropbearkey
DROPBEARCONVERT=/usr/local/bin/dropbearconvert

# Tools used by multiple tests
NC=nc
OPENSSL_BIN="${OPENSSL_BIN:-openssl}"

if [ "x$TEST_SSH_SSH" != "x" ]; then
	SSH="${TEST_SSH_SSH}"
fi
if [ "x$TEST_SSH_SSHD" != "x" ]; then
	SSHD="${TEST_SSH_SSHD}"
fi
if [ "x$TEST_SSH_SSHAGENT" != "x" ]; then
	SSHAGENT="${TEST_SSH_SSHAGENT}"
fi
if [ "x$TEST_SSH_SSHADD" != "x" ]; then
	SSHADD="${TEST_SSH_SSHADD}"
fi
if [ "x$TEST_SSH_SSHKEYGEN" != "x" ]; then
	SSHKEYGEN="${TEST_SSH_SSHKEYGEN}"
fi
if [ "x$TEST_SSH_SSHKEYSCAN" != "x" ]; then
	SSHKEYSCAN="${TEST_SSH_SSHKEYSCAN}"
fi
if [ "x$TEST_SSH_SFTP" != "x" ]; then
	SFTP="${TEST_SSH_SFTP}"
fi
if [ "x$TEST_SSH_SFTPSERVER" != "x" ]; then
	SFTPSERVER="${TEST_SSH_SFTPSERVER}"
fi
if [ "x$TEST_SSH_SCP" != "x" ]; then
	SCP="${TEST_SSH_SCP}"
fi
if [ "x$TEST_SSH_PLINK" != "x" ]; then
	PLINK="${TEST_SSH_PLINK}"
fi
if [ "x$TEST_SSH_PUTTYGEN" != "x" ]; then
	PUTTYGEN="${TEST_SSH_PUTTYGEN}"
fi
if [ "x$TEST_SSH_CONCH" != "x" ]; then
	CONCH="${TEST_SSH_CONCH}"
fi
if [ "x$TEST_SSH_DROPBEAR" != "x" ]; then
	DROPBEAR="${TEST_SSH_DROPBEAR}"
fi
if [ "x$TEST_SSH_DBCLIENT" != "x" ]; then
	DBCLIENT="${TEST_SSH_DBCLIENT}"
fi
if [ "x$TEST_SSH_DROPBEARKEY" != "x" ]; then
	DROPBEARKEY="${TEST_SSH_DROPBEARKEY}"
fi
if [ "x$TEST_SSH_DROPBEARCONVERT" != "x" ]; then
	DROPBEARCONVERT="${TEST_SSH_DROPBEARCONVERT}"
fi
if [ "x$TEST_SSH_PKCS11_HELPER" != "x" ]; then
	SSH_PKCS11_HELPER="${TEST_SSH_PKCS11_HELPER}"
fi
if [ "x$TEST_SSH_SK_HELPER" != "x" ]; then
	SSH_SK_HELPER="${TEST_SSH_SK_HELPER}"
fi
if [ "x$TEST_SSH_OPENSSL" != "x" ]; then
	OPENSSL_BIN="${TEST_SSH_OPENSSL}"
fi

# Path to sshd must be absolute for rexec
case "$SSHD" in
/*) ;;
*) SSHD=`which $SSHD` ;;
esac

case "$SSHAGENT" in
/*) ;;
*) SSHAGENT=`which $SSHAGENT` ;;
esac

# Logfiles.
# SSH_LOGFILE should be the debug output of ssh(1) only
# SSHD_LOGFILE should be the debug output of sshd(8) only
# REGRESS_LOGFILE is the log of progress of the regress test itself.
# TEST_SSH_LOGDIR will contain datestamped logs of all binaries run in
# chronological order.
if [ "x$TEST_SSH_LOGDIR" = "x" ]; then
	TEST_SSH_LOGDIR=$OBJ/log
	mkdir -p $TEST_SSH_LOGDIR
fi
if [ "x$TEST_SSH_LOGFILE" = "x" ]; then
	TEST_SSH_LOGFILE=$OBJ/ssh.log
fi
if [ "x$TEST_SSHD_LOGFILE" = "x" ]; then
	TEST_SSHD_LOGFILE=$OBJ/sshd.log
fi
if [ "x$TEST_REGRESS_LOGFILE" = "x" ]; then
	TEST_REGRESS_LOGFILE=$OBJ/regress.log
fi

# If set, keep track of successful tests and skip them them if we've
# previously completed that test.
if [ "x$TEST_REGRESS_CACHE_DIR" != "x" ]; then
	if [ ! -d "$TEST_REGRESS_CACHE_DIR" ]; then
		mkdir -p "$TEST_REGRESS_CACHE_DIR"
	fi
	TEST="`basename $SCRIPT .sh`"
	CACHE="${TEST_REGRESS_CACHE_DIR}/${TEST}.cache"
	for i in ${SSH} ${SSHD} ${SSHAGENT} ${SSHADD} ${SSHKEYGEN} ${SCP} \
	    ${SFTP} ${SFTPSERVER} ${SSHKEYSCAN}; do
		case $i in
		/*)	bin="$i" ;;
		*)	bin="`which $i`" ;;
		esac
		if [ "$bin" -nt "$CACHE" ]; then
			rm -f "$CACHE"
		fi
	done
	if [ -f "$CACHE" ]; then
		echo ok cached $CACHE
		exit 0
	fi
fi

# truncate logfiles
>$TEST_REGRESS_LOGFILE

# Create ssh and sshd wrappers with logging.  These create a datestamped
# unique file for every invocation so that we can retain all logs from a
# given test no matter how many times it's invoked.  It also leaves a
# symlink with the original name for tests (and people) who look for that.

# For ssh, e can't just specify "SSH=ssh -E..." because sftp and scp don't
# handle spaces in arguments.  scp and sftp like to use -q so we remove those
# to preserve our debug logging.  In the rare instance where -q is desirable
# -qq is equivalent and is not removed.
SSHLOGWRAP=$OBJ/ssh-log-wrapper.sh
cat >$SSHLOGWRAP <<EOD
#!/bin/sh
timestamp="\`$OBJ/timestamp\`"
logfile="${TEST_SSH_LOGDIR}/\${timestamp}.ssh.\$\$.log"
echo "Executing: ${SSH} \$@" log \${logfile} >>$TEST_REGRESS_LOGFILE
echo "Executing: ${SSH} \$@" >>\${logfile}
for i in "\$@";do shift;case "\$i" in -q):;; *) set -- "\$@" "\$i";;esac;done
rm -f $TEST_SSH_LOGFILE
ln -f -s \${logfile} $TEST_SSH_LOGFILE
exec ${SSH} -E\${logfile} "\$@"
EOD

chmod a+rx $OBJ/ssh-log-wrapper.sh
REAL_SSH="$SSH"
REAL_SSHD="$SSHD"
SSH="$SSHLOGWRAP"

SSHDLOGWRAP=$OBJ/sshd-log-wrapper.sh
cat >$SSHDLOGWRAP <<EOD
#!/bin/sh
timestamp="\`$OBJ/timestamp\`"
logfile="${TEST_SSH_LOGDIR}/\${timestamp}.sshd.\$\$.log"
rm -f $TEST_SSHD_LOGFILE
ln -f -s \${logfile} $TEST_SSHD_LOGFILE
echo "Executing: ${SSHD} \$@" log \${logfile} >>$TEST_REGRESS_LOGFILE
echo "Executing: ${SSHD} \$@" >>\${logfile}
exec ${SSHD} -E\${logfile} "\$@"
EOD
chmod a+rx $OBJ/sshd-log-wrapper.sh

ssh_logfile ()
{
	tool="$1"
	timestamp="`$OBJ/timestamp`"
	logfile="${TEST_SSH_LOGDIR}/${timestamp}.$tool.$$.log"
	echo "Logging $tool to log \${logfile}" >>$TEST_REGRESS_LOGFILE
	echo $logfile
}

# Some test data.  We make a copy because some tests will overwrite it.
# The tests may assume that $DATA exists and is writable and $COPY does
# not exist.  Tests requiring larger data files can call increase_datafile_size
# [kbytes] to ensure the file is at least that large.
DATANAME=data
DATA=$OBJ/${DATANAME}
cat ${SSHAGENT} >${DATA}
COPY=$OBJ/copy
rm -f ${COPY}

increase_datafile_size()
{
	while [ `du -k ${DATA} | cut -f1` -lt $1 ]; do
		cat ${SSHAGENT} >>${DATA}
	done
}

# these should be used in tests
export SSH SSHD SSHAGENT SSHADD SSHKEYGEN SSHKEYSCAN SFTP SFTPSERVER SCP
export SSH_PKCS11_HELPER SSH_SK_HELPER
#echo $SSH $SSHD $SSHAGENT $SSHADD $SSHKEYGEN $SSHKEYSCAN $SFTP $SFTPSERVER $SCP

stop_sshd ()
{
	if [ -f $PIDFILE ]; then
		pid=`$SUDO cat $PIDFILE`
		if [ "X$pid" = "X" ]; then
			echo no sshd running
		else
			if [ $pid -lt 2 ]; then
				echo bad pid for sshd: $pid
			else
				$SUDO kill $pid
				trace "wait for sshd to exit"
				i=0;
				while [ -f $PIDFILE -a $i -lt 5 ]; do
					i=`expr $i + 1`
					sleep $i
				done
				if test -f $PIDFILE; then
					if $SUDO kill -0 $pid; then
						echo "sshd didn't exit " \
						    "port $PORT pid $pid"
					else
						echo "sshd died without cleanup"
					fi
					exit 1
				fi
			fi
		fi
	fi
}

# helper
cleanup ()
{
	if [ "x$SSH_PID" != "x" ]; then
		if [ $SSH_PID -lt 2 ]; then
			echo bad pid for ssh: $SSH_PID
		else
			kill $SSH_PID
		fi
	fi
	stop_sshd
	if [ ! -z "$TEST_SSH_ELAPSED_TIMES" ]; then
		now=`date '+%s'`
		elapsed=$(($now - $STARTTIME))
		echo elapsed $elapsed `basename $SCRIPT .sh`
	fi
}

start_debug_log ()
{
	echo "trace: $@" >>$TEST_REGRESS_LOGFILE
	if [ -d "$TEST_SSH_LOGDIR" ]; then
		rm -f $TEST_SSH_LOGDIR/*
	fi
}

save_debug_log ()
{
	testname=`echo $tid | tr ' ' _`
	tarname="$OBJ/failed-$testname-logs.tar"

	if [ ! -z "$SUDO" ] ; then
		$SUDO chown -R $USER $TEST_SSH_LOGDIR $TEST_REGRESS_LOGFILE \
		    $TEST_SSH_LOGFILE $TEST_SSHD_LOGFILE
	fi
	echo $@ >>$TEST_REGRESS_LOGFILE
	echo $@ >>$TEST_SSH_LOGFILE
	echo $@ >>$TEST_SSHD_LOGFILE
	echo "Saving debug logs to $tarname" >>$TEST_REGRESS_LOGFILE
	(cat $TEST_REGRESS_LOGFILE; echo) >>$OBJ/failed-regress.log
	(cat $TEST_SSH_LOGFILE; echo) >>$OBJ/failed-ssh.log
	(cat $TEST_SSHD_LOGFILE; echo) >>$OBJ/failed-sshd.log

	# Save all logfiles in a tarball.
	(cd $OBJ &&
	  logfiles=""
	  for i in $TEST_REGRESS_LOGFILE $TEST_SSH_LOGFILE $TEST_SSHD_LOGFILE \
	    $TEST_SSH_LOGDIR; do
		if [ -e "`basename $i`" ]; then
			logfiles="$logfiles `basename $i`"
		else
			logfiles="$logfiles $i"
		fi
	  done
	  tar cf "$tarname" $logfiles)
}

trace ()
{
	start_debug_log $@
	if [ "X$TEST_SSH_TRACE" = "Xyes" ]; then
		echo "$@"
	fi
}

verbose ()
{
	start_debug_log $@
	if [ "X$TEST_SSH_QUIET" != "Xyes" ]; then
		echo "$@"
	fi
}

fail ()
{
	save_debug_log "FAIL: $@"
	RESULT=1
	echo "$@"
	if test "x$TEST_SSH_FAIL_FATAL" != "x" ; then
		cleanup
		exit $RESULT
	fi
}

fatal ()
{
	save_debug_log "FATAL: $@"
	printf "FATAL: "
	fail "$@"
	cleanup
	exit $RESULT
}

# Skip remaining tests in script.
skip ()
{
	echo "SKIPPED: $@"
	cleanup
	exit $RESULT
}

maybe_add_scp_path_to_sshd ()
{
	# If we're testing a non-installed scp, add its directory to sshd's
	# PATH so we can test it.  We don't do this for all tests as it
	# breaks the SetEnv tests.
	case "$SCP" in
	/*)	PATH_WITH_SCP="`dirname $SCP`:$PATH"
		echo "	SetEnv PATH='$PATH_WITH_SCP'" >>$OBJ/sshd_config
		echo "	SetEnv PATH='$PATH_WITH_SCP'" >>$OBJ/sshd_proxy ;;
	esac
}

RESULT=0
PIDFILE=$OBJ/pidfile

trap fatal 3 2

# create server config
cat << EOF > $OBJ/sshd_config
	Port			$PORT
	AddressFamily		inet
	ListenAddress		127.0.0.1
	#ListenAddress		::1
	PidFile			$PIDFILE
	AuthorizedKeysFile	$OBJ/authorized_keys_%u
	LogLevel		DEBUG3
	AcceptEnv		_XXX_TEST_*
	AcceptEnv		_XXX_TEST
	Subsystem	sftp	$SFTPSERVER
EOF

# This may be necessary if /usr/src and/or /usr/obj are group-writable,
# but if you aren't careful with permissions then the unit tests could
# be abused to locally escalate privileges.
if [ ! -z "$TEST_SSH_UNSAFE_PERMISSIONS" ]; then
	echo "	StrictModes no" >> $OBJ/sshd_config
else
	# check and warn if excessive permissions are likely to cause failures.
	unsafe=""
	dir="${OBJ}"
	while test ${dir} != "/"; do
		if test -d "${dir}" && ! test -h "${dir}"; then
			perms=`ls -ld ${dir}`
			case "${perms}" in
			?????w????*|????????w?*) unsafe="${unsafe} ${dir}" ;;
			esac
		fi
		dir=`dirname ${dir}`
	done
	if ! test  -z "${unsafe}"; then
		cat <<EOD

WARNING: Unsafe (group or world writable) directory permissions found:
${unsafe}

These could be abused to locally escalate privileges.  If you are
sure that this is not a risk (eg there are no other users), you can
bypass this check by setting TEST_SSH_UNSAFE_PERMISSIONS=1

EOD
	fi
fi

if [ ! -z "$TEST_SSH_MODULI_FILE" ]; then
	trace "adding modulifile='$TEST_SSH_MODULI_FILE' to sshd_config"
	echo "	ModuliFile '$TEST_SSH_MODULI_FILE'" >> $OBJ/sshd_config
fi

if [ ! -z "$TEST_SSH_SSHD_CONFOPTS" ]; then
	trace "adding sshd_config option $TEST_SSH_SSHD_CONFOPTS"
	echo "$TEST_SSH_SSHD_CONFOPTS" >> $OBJ/sshd_config
fi

# server config for proxy connects
cp $OBJ/sshd_config $OBJ/sshd_proxy

# allow group-writable directories in proxy-mode
echo 'StrictModes no' >> $OBJ/sshd_proxy

# create client config
cat << EOF > $OBJ/ssh_config
Host *
	Hostname		127.0.0.1
	HostKeyAlias		localhost-with-alias
	Port			$PORT
	User			$USER
	GlobalKnownHostsFile	$OBJ/known_hosts
	UserKnownHostsFile	$OBJ/known_hosts
	PubkeyAuthentication	yes
	ChallengeResponseAuthentication	no
	PasswordAuthentication	no
	BatchMode		yes
	StrictHostKeyChecking	yes
	LogLevel		DEBUG3
EOF

if [ ! -z "$TEST_SSH_SSH_CONFOPTS" ]; then
	trace "adding ssh_config option $TEST_SSH_SSH_CONFOPTS"
	echo "$TEST_SSH_SSH_CONFOPTS" >> $OBJ/ssh_config
fi

rm -f $OBJ/known_hosts $OBJ/authorized_keys_$USER

SSH_SK_PROVIDER=
if [ -f "${SRC}/misc/sk-dummy/obj/sk-dummy.so" ] ; then
	SSH_SK_PROVIDER="${SRC}/misc/sk-dummy/obj/sk-dummy.so"
elif [ -f "${SRC}/misc/sk-dummy/sk-dummy.so" ] ; then
	SSH_SK_PROVIDER="${SRC}/misc/sk-dummy/sk-dummy.so"
fi
export SSH_SK_PROVIDER

if ! test -z "$SSH_SK_PROVIDER"; then
	EXTRA_AGENT_ARGS='-P/*' # XXX want realpath(1)...
	echo "SecurityKeyProvider $SSH_SK_PROVIDER" >> $OBJ/ssh_config
	echo "SecurityKeyProvider $SSH_SK_PROVIDER" >> $OBJ/sshd_config
	echo "SecurityKeyProvider $SSH_SK_PROVIDER" >> $OBJ/sshd_proxy
fi
export EXTRA_AGENT_ARGS

maybe_filter_sk() {
	if test -z "$SSH_SK_PROVIDER" ; then
		grep -v ^sk
	else
		cat
	fi
}

SSH_KEYTYPES=`$SSH -Q key-plain | maybe_filter_sk`
SSH_HOSTKEY_TYPES=`$SSH -Q key-plain | maybe_filter_sk`

for t in ${SSH_KEYTYPES}; do
	# generate user key
	if [ ! -f $OBJ/$t ] || [ ${SSHKEYGEN} -nt $OBJ/$t ]; then
		trace "generating key type $t"
		rm -f $OBJ/$t
		${SSHKEYGEN} -q -N '' -t $t  -f $OBJ/$t ||\
			fail "ssh-keygen for $t failed"
	else
		trace "using cached key type $t"
	fi

	# setup authorized keys
	cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
	echo IdentityFile $OBJ/$t >> $OBJ/ssh_config
done

for t in ${SSH_HOSTKEY_TYPES}; do
	# known hosts file for client
	(
		printf 'localhost-with-alias,127.0.0.1,::1 '
		cat $OBJ/$t.pub
	) >> $OBJ/known_hosts

	# use key as host key, too
	(umask 077; $SUDO cp $OBJ/$t $OBJ/host.$t)
	echo HostKey $OBJ/host.$t >> $OBJ/sshd_config

	# don't use SUDO for proxy connect
	echo HostKey $OBJ/$t >> $OBJ/sshd_proxy
done
chmod 644 $OBJ/authorized_keys_$USER

# Activate Twisted Conch tests if the binary is present
REGRESS_INTEROP_CONCH=no
if test -x "$CONCH" ; then
	REGRESS_INTEROP_CONCH=yes
fi

# If PuTTY is present, new enough and we are running a PuTTY test, prepare
# keys and configuration.
REGRESS_INTEROP_PUTTY=no
if test -x "$PUTTYGEN" -a -x "$PLINK" &&
    "$PUTTYGEN" --help 2>&1 | grep -- --new-passphrase >/dev/null; then
	REGRESS_INTEROP_PUTTY=yes
fi
case "$SCRIPT" in
*putty*)	;;
*)		REGRESS_INTEROP_PUTTY=no ;;
esac

if test "$REGRESS_INTEROP_PUTTY" = "yes" ; then
	mkdir -p ${OBJ}/.putty

	# Add a PuTTY key to authorized_keys
	rm -f ${OBJ}/putty.rsa2
	if ! "$PUTTYGEN" -t rsa -o ${OBJ}/putty.rsa2 \
	    --random-device=/dev/urandom \
	    --new-passphrase /dev/null < /dev/null > /dev/null; then
		echo "Your installed version of PuTTY is too old to support --new-passphrase, skipping test" >&2
		exit 1
	fi
	"$PUTTYGEN" -O public-openssh ${OBJ}/putty.rsa2 \
	    >> $OBJ/authorized_keys_$USER

	# Convert rsa2 host key to PuTTY format
	cp $OBJ/ssh-rsa $OBJ/ssh-rsa_oldfmt
	${SSHKEYGEN} -p -N '' -m PEM -f $OBJ/ssh-rsa_oldfmt >/dev/null
	${SRC}/ssh2putty.sh 127.0.0.1 $PORT $OBJ/ssh-rsa_oldfmt > \
	    ${OBJ}/.putty/sshhostkeys
	${SRC}/ssh2putty.sh 127.0.0.1 22 $OBJ/ssh-rsa_oldfmt >> \
	    ${OBJ}/.putty/sshhostkeys
	rm -f $OBJ/ssh-rsa_oldfmt

	# Setup proxied session
	mkdir -p ${OBJ}/.putty/sessions
	rm -f ${OBJ}/.putty/sessions/localhost_proxy
	echo "Protocol=ssh" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "HostName=127.0.0.1" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "PortNumber=$PORT" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "ProxyMethod=5" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "ProxyTelnetCommand=${OBJ}/sshd-log-wrapper.sh -i -f $OBJ/sshd_proxy" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "ProxyLocalhost=1" >> ${OBJ}/.putty/sessions/localhost_proxy
fi

REGRESS_INTEROP_DROPBEAR=no
if test -x "$DROPBEARKEY" -a -x "$DBCLIENT" -a -x "$DROPBEARCONVERT"; then
	REGRESS_INTEROP_DROPBEAR=yes
fi
case "$SCRIPT" in
*dropbear*)	;;
*)		REGRESS_INTEROP_DROPBEAR=no ;;
esac

if test "$REGRESS_INTEROP_DROPBEAR" = "yes" ; then
	trace Create dropbear keys and add to authorized_keys
	mkdir -p $OBJ/.dropbear
	for i in rsa ecdsa ed25519 dss; do
		if [ ! -f "$OBJ/.dropbear/id_$i" ]; then
			($DROPBEARKEY -t $i -f $OBJ/.dropbear/id_$i
			$DROPBEARCONVERT dropbear openssh \
			    $OBJ/.dropbear/id_$i $OBJ/.dropbear/ossh.id_$i
			) > /dev/null 2>&1
		fi
		$SSHKEYGEN -y -f $OBJ/.dropbear/ossh.id_$i \
		   >>$OBJ/authorized_keys_$USER
	done
fi

# create a proxy version of the client config
(
	cat $OBJ/ssh_config
	echo proxycommand ${SUDO} env SSH_SK_HELPER=\"$SSH_SK_HELPER\" ${OBJ}/sshd-log-wrapper.sh -i -f $OBJ/sshd_proxy
) > $OBJ/ssh_proxy

# check proxy config
${SSHD} -t -f $OBJ/sshd_proxy	|| fatal "sshd_proxy broken"

# extract proxycommand into separate shell script for use by Dropbear.
echo '#!/bin/sh' >$OBJ/ssh_proxy.sh
awk '/^proxycommand/' $OBJ/ssh_proxy | sed 's/^proxycommand//' \
   >>$OBJ/ssh_proxy.sh
chmod a+x $OBJ/ssh_proxy.sh

start_sshd ()
{
	# start sshd
	logfile="${TEST_SSH_LOGDIR}/sshd.`$OBJ/timestamp`.$$.log"
	$SUDO ${SSHD} -f $OBJ/sshd_config "$@" -t || fatal "sshd_config broken"
	$SUDO env SSH_SK_HELPER="$SSH_SK_HELPER" \
	    ${SSHD} -f $OBJ/sshd_config "$@" -E$logfile
	echo "trace: Started sshd as daemon, logfile $logfile" >>$TEST_REGRESS_LOGFILE

	trace "wait for sshd"
	i=0;
	while [ ! -f $PIDFILE -a $i -lt 10 ]; do
		i=`expr $i + 1`
		sleep $i
	done

	test -f $PIDFILE || fatal "no sshd running on port $PORT"
}

# source test body
. $SCRIPT

# kill sshd
cleanup
if [ $RESULT -eq 0 ]; then
	verbose ok $tid
	if [ "x$CACHE" != "x" ]; then
		touch "$CACHE"
	fi
else
	echo failed $tid
fi
exit $RESULT
