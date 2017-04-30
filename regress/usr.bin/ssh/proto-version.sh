#	$OpenBSD: proto-version.sh,v 1.5 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="sshd version with different protocol combinations"

# we just start sshd in inetd mode and check the banner
check_version ()
{
	expect=$2
	banner=`printf '' | ${SSHD} -i -f ${OBJ}/sshd_proxy`
	case ${banner} in
	SSH-1.99-*)
		proto=199
		;;
	SSH-2.0-*)
		proto=20
		;;
	SSH-1.5-*)
		proto=15
		;;
	*)
		proto=0
		;;
	esac
	if [ ${expect} -ne ${proto} ]; then
		fail "wrong protocol version ${banner}"
	fi
}

check_version	20
