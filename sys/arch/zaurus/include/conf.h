/*	$OpenBSD: conf.h,v 1.1 2004/12/31 00:04:35 drahn Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _ZAURUS_CONF_H
#define	_ZAURUS_CONF_H

/*
 * ZAURUS specific device includes go in here
 */
#include "fcom.h"

#define	CONF_HAVE_PCI
#define	CONF_HAVE_USB
#define	CONF_HAVE_SCSIPI
#define	CONF_HAVE_WSCONS

#include <arm/conf.h>

#endif	/* _ZAURUS_CONF_H */
