#!/bin/ksh
#	$OpenBSD: ovs.sh,v 1.2 2019/05/29 08:52:50 claudio Exp $

set -e

BGPD=$1
BGPDCONFIGDIR=$2
RDOMAIN1=$3

error_notify() {
	echo cleanup
	pkill -T ${RDOMAIN1} bgpd || true
	sleep 1
	echo cleanup rdomain
	route -qn -T ${RDOMAIN1} flush || true
	echo cleanup interfaces
	ifconfig mpe${RDOMAIN1} destroy || true
	ifconfig lo${RDOMAIN1} destroy || true
	if [ $1 -ne 0 ]; then
		echo FAILED
		exit 1
	else
		echo SUCCESS
	fi
}

trap 'error_notify $?' EXIT

echo check if rdomains are busy
if /sbin/ifconfig lo${RDOMAIN1} > /dev/null 2>&1; then
    echo routing domain ${RDOMAIN1} is already used >&2; exit 1;
fi

echo setup
ifconfig mpe${RDOMAIN1} rdomain ${RDOMAIN1} mplslabel 42
ifconfig lo${RDOMAIN1} inet 127.0.0.1/8

route -T ${RDOMAIN1} exec ${BGPD} \
	-v -f ${BGPDCONFIGDIR}/bgpd.mrt.conf

sleep 2

pkill -USR1 -T ${RDOMAIN1} -U 0 bgpd

sleep 2

for i in table-v2 table-mp table; do
	echo test $i
	bgpctl show mrt detail file mrt-$i.mrt | \
		grep -v 'Last update:' > mrt-$i.out
	diff -u ${BGPDCONFIGDIR}/mrt-$i.ok mrt-$i.out
done

exit 0
