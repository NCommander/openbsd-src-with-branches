/*	$OpenBSD: atoo.c,v 1.3 1997/04/01 07:34:57 todd Exp $	*/

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*  atoo  --  convert ascii to octal
 *
 *  Usge:  i = atoo (string);
 *	unsigned int i;
 *	char *string;
 *
 *  Atoo converts the value contained in "string" into an
 *  unsigned integer, assuming that the value represents
 *  an octal number.
 *
 *  HISTORY
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Rewritten for VAX.
 *
 */
#include "supcdefs.h"
#include "supextern.h"

unsigned int atoo(ap)
	char *ap;
{
	unsigned int n;
	char *p;

	p = ap;
	n = 0;
	while (*p == ' ' || *p == '\t')
		p++;
	while (*p >= '0' && *p <= '7')
		n = n * 8 + *p++ - '0';
	return(n);
}
