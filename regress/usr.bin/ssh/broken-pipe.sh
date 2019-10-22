#	$OpenBSD: broken-pipe.sh,v 1.5 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="broken pipe test"

for i in 1 2 3 4; do
	${SSH} -F $OBJ/ssh_config_config nexthost echo $i 2> /dev/null | true
	r=$?
	if [ $r -ne 0 ]; then
		fail "broken pipe returns $r"
	fi
done
