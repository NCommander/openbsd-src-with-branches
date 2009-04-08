/*	$OpenBSD$	*/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#define FUNCNAME	wcstod
typedef double		float_type;
#define STRTOD_FUNC	strtod

#include "_wcstod.h"
