#
#	$OpenBSD: start_rdconfig.sh,v 1.3 1995/09/30 20:00:47 briggs Exp $
echo rdconfig ${1} ${2}
rdconfig ${1} ${2} &
echo  $! >rd.pid 

