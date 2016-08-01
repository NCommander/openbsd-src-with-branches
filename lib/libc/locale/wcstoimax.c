/*	$OpenBSD: wcstol.c,v 1.2 2005/08/08 08:05:35 espie Exp $	*/
/* $NetBSD: wcstol.c,v 1.2 2003/03/11 09:21:23 tshiozak Exp $ */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "wctoint.h"

#define	FUNCNAME	wcstoimax
typedef intmax_t	int_type;
#define	MIN_VALUE	INTMAX_MIN
#define	MAX_VALUE	INTMAX_MAX

#include "_wcstol.h"
