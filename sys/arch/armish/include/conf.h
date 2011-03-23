/*	$OpenBSD: conf.h,v 1.2 2010/11/28 20:49:45 miod Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _MACHINE_CONF_H_
#define	_MACHINE_CONF_H_

#include <sys/conf.h>

/*
 * ARMISH specific device includes go in here
 */

#define	CONF_HAVE_GPIO

#include <arm/conf.h>

#endif	/* _MACHINE_CONF_H_ */
