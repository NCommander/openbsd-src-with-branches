#	$OpenBSD: exit-status.sh,v 1.6 2002/03/15 13:08:56 markus Exp $
#	Placed in the Public Domain.

tid="stderr data after eof"

DATA=/etc/motd
DATA=${OBJ}/data
COPY=${OBJ}/copy

# setup data
rm -f ${DATA} ${COPY}
cp /dev/null ${DATA}
for i in 1 2 3 4 5 6; do
	(date;echo $i) | md5 >> ${DATA}
done

${SSH} -2 -F $OBJ/ssh_proxy otherhost \
	exec sh -c \'"exec > /dev/null; sleep 2; cat ${DATA} 1>&2 $s"\' \
	2> ${COPY}
r=$?
if [ $r -ne 0 ]; then
	fail "ssh failed with exit code $r"
fi
egrep 'Disconnecting: Received extended_data after EOF' ${COPY} &&
	fail "ext data received after eof"
cmp ${DATA} ${COPY}	|| fail "stderr corrupt"

rm -f ${DATA} ${COPY}
