/*	$OpenBSD: compress.c,v 1.4 2002/03/12 00:25:57 millert Exp $	*/
/* compress.c -- compress a memory buffer
 * Copyright (C) 1995-2002 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */


#define ZLIB_INTERNAL
#include "zlib.h"

uLong ZEXPORT compressBound (sourceLen)
    uLong sourceLen;
{
    return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + 11;
}
