/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: stdio.h,v 1.4 2020/01/26 11:26:30 florian Exp $ */

#ifndef ISC_STDIO_H
#define ISC_STDIO_H 1

/*! \file isc/stdio.h */

/*%
 * These functions are wrappers around the corresponding stdio functions.
 *
 * They return a detailed error code in the form of an an isc_result_t.  ANSI C
 * does not guarantee that stdio functions set errno, hence these functions
 * must use platform dependent methods (e.g., the POSIX errno) to construct the
 * error code.
 */

#include <stdio.h>

#include <isc/lang.h>
#include <isc/result.h>

ISC_LANG_BEGINDECLS

/*% Open */
isc_result_t
isc_stdio_open(const char *filename, const char *mode, FILE **fp);

ISC_LANG_ENDDECLS

#endif /* ISC_STDIO_H */
