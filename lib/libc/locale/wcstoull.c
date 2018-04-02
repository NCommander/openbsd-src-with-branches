/*	$OpenBSD: wcstoull.c,v 1.1 2005/07/01 08:59:27 espie Exp $	*/
/*	$NetBSD: wcstoull.c,v 1.1 2003/03/11 09:21:24 tshiozak Exp $	*/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "wctoint.h"

#define	FUNCNAME	wcstoull
typedef unsigned long long int uint_type;
#define	MAX_VALUE	ULLONG_MAX

#include "_wcstoul.h"
