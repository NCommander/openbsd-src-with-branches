#	$OpenBSD: keyscan.sh,v 1.6 2017/04/30 23:34:55 djm Exp $
#	Placed in the Public Domain.

tid="keyscan"

KEYTYPES=`${SSH} -Q key-plain`
for i in $KEYTYPES; do
	if [ -z "$algs" ]; then
		algs="$i"
	else
		algs="$algs,$i"
	fi
done
echo "HostKeyAlgorithms $algs" >> sshd_config

cat sshd_config

start_sshd

for t in $KEYTYPES; do
	trace "keyscan type $t"
	${SSHKEYSCAN} -t $t -p $PORT 127.0.0.1 127.0.0.1 127.0.0.1 \
		> /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-keyscan -t $t failed with: $r"
	fi
done
