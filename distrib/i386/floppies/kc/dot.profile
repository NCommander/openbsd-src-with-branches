#	$OpenBSD$

PATH=/sbin:/bin:/
export PATH

if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES

	echo -n	"Enter 'copy_kernel' at the prompt to copy a kernel to your "
	echo	"hard disk,"
	echo	"'reboot' to reboot the system, or 'halt' to halt the system."
	echo	""
fi
