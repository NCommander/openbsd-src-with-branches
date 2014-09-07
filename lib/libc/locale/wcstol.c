/*	$OpenBSD: wcstol.c,v 1.1 2005/07/01 08:59:27 espie Exp $	*/
/* $NetBSD: wcstol.c,v 1.2 2003/03/11 09:21:23 tshiozak Exp $ */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "wctoint.h"

#define	FUNCNAME	wcstol
typedef long int_type;
#define	MIN_VALUE	LONG_MIN
#define	MAX_VALUE	LONG_MAX

#include "_wcstol.h"
