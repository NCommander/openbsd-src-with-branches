/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Sendmail: sm_os_aix.h,v 1.8 2001/10/01 15:04:26 ca Exp $
 */

/*
**  sm_os_aix.h -- platform definitions for AIX
*/

#define SM_OS_NAME	"aix"

#ifndef SM_CONF_SHM
# define SM_CONF_SHM	1
#endif /* SM_CONF_SHM */
#ifndef SM_CONF_SEM
# define SM_CONF_SEM	2
#endif /* SM_CONF_SEM */
#ifndef SM_CONF_MSG
# define SM_CONF_MSG	1
#endif /* SM_CONF_MSG */

/* AIX 3 doesn't have a prototype for syslog()? */
#ifdef _AIX3
# ifndef _AIX4
#  ifndef SM_CONF_SYSLOG
#   define SM_CONF_SYSLOG	0
#  endif /* SM_CONF_SYSLOG */
# endif /* ! _AIX4 */
#endif /* _AIX3 */

/* can't set real gid */
#ifndef SM_CONF_CANT_SETRGID
# define SM_CONF_CANT_SETRGID	1
#endif /* SM_CONF_CANT_SETRGID */
