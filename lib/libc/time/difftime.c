/*	$OpenBSD: difftime.c,v 1.9 2005/08/08 08:05:38 espie Exp $ */
/* This file is placed in the public domain by Ted Unangst. */

#include "private.h"

double
difftime(time_t time1, time_t time0)
{
	return time1 - time0;
}
