/*?$OpenBSD$?*/

#include <sys/types.h>
#include <unistd.h>
#include <proto/dos.h>

/* cheap sleep, but we don't need a good one */
u_int
sleep(u_int n)
{
	(void)Delay(50 * n);	
	return 0;
}
