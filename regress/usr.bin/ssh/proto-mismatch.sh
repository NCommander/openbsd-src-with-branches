#	$OpenBSD: proto-mismatch.sh,v 1.4 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="protocol version mismatch"

mismatch ()
{
	client=$2
	banner=`echo ${client} | ${SSHD} -i -f ${OBJ}/sshd_proxy`
	r=$?
	trace "sshd prints ${banner}"
	if [ $r -ne 255 ]; then
		fail "sshd prints ${banner} but accepts version ${client}"
	fi
}

mismatch	SSH-1.5-HALLO
