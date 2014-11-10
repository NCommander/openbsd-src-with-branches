/*	$OpenBSD: config.h,v 1.10 2014/11/06 10:48:52 bentley Exp $	*/

/* Define if you want a debugging version. */
/* #undef DEBUG */

/* Define if you have fcntl(2) style locking. */
/* #undef HAVE_LOCK_FCNTL */

/* Define if you have flock(2) style locking. */
#define HAVE_LOCK_FLOCK 1
