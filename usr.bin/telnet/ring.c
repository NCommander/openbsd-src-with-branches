/*	$OpenBSD: ring.c,v 1.10 2014/07/20 10:32:23 jsg Exp $	*/
/*	$NetBSD: ring.c,v 1.7 1996/02/28 21:04:07 thorpej Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>
#include "ring.h"

/*
 * This defines a structure for a ring buffer.
 *
 * The circular buffer has two parts:
 *(((
 *	full:	[consume, supply)
 *	empty:	[supply, consume)
 *]]]
 *
 */

/* Internal macros */
#define	ring_subtract(d,a,b)	(((a)-(b) >= 0)? \
					(a)-(b): (((a)-(b))+(d)->size))

#define	ring_increment(d,a,c)	(((a)+(c) < (d)->top)? \
					(a)+(c) : (((a)+(c))-(d)->size))

#define	ring_decrement(d,a,c)	(((a)-(c) >= (d)->bottom)? \
					(a)-(c) : (((a)-(c))-(d)->size))


/*
 * The following is a clock, used to determine full, empty, etc.
 *
 * There is some trickiness here.  Since the ring buffers are initialized
 * to ZERO on allocation, we need to make sure, when interpreting the
 * clock, that when the times are EQUAL, then the buffer is FULL.
 */
static unsigned long ring_clock = 0;


#define	ring_empty(d) (((d)->consume == (d)->supply) && \
				((d)->consumetime >= (d)->supplytime))
#define	ring_full(d) (((d)->supply == (d)->consume) && \
				((d)->supplytime > (d)->consumetime))


/* Buffer state transition routines */

void
ring_init(Ring *ring, unsigned char *buffer, int count)
{
    memset(ring, 0, sizeof *ring);

    ring->size = count;

    ring->supply = ring->consume = ring->bottom = buffer;

    ring->top = ring->bottom+ring->size;
}

/* Mark routines */

/*
 * Mark the most recently supplied byte.
 */

void
ring_mark(Ring *ring)
{
    ring->mark = ring_decrement(ring, ring->supply, 1);
}

/*
 * Is the ring pointing to the mark?
 */

int
ring_at_mark(Ring *ring)
{
    if (ring->mark == ring->consume) {
	return 1;
    } else {
	return 0;
    }
}

/*
 * Clear any mark set on the ring.
 */

void
ring_clear_mark(Ring *ring)
{
    ring->mark = NULL;
}

/*
 * Add characters from current segment to ring buffer.
 */
void
ring_supplied(Ring *ring, int count)
{
    ring->supply = ring_increment(ring, ring->supply, count);
    ring->supplytime = ++ring_clock;
}

/*
 * We have just consumed "c" bytes.
 */
void
ring_consumed(Ring *ring, int count)
{
    if (count == 0)	/* don't update anything */
	return;

    if (ring->mark &&
		(ring_subtract(ring, ring->mark, ring->consume) < count)) {
	ring->mark = NULL;
    }
    ring->consume = ring_increment(ring, ring->consume, count);
    ring->consumetime = ++ring_clock;
    /*
     * Try to encourage "ring_empty_consecutive()" to be large.
     */
    if (ring_empty(ring)) {
	ring->consume = ring->supply = ring->bottom;
    }
}


/* Buffer state query routines */


/* Number of bytes that may be supplied */
int
ring_empty_count(Ring *ring)
{
    if (ring_empty(ring)) {	/* if empty */
	    return ring->size;
    } else {
	return ring_subtract(ring, ring->consume, ring->supply);
    }
}

/* number of CONSECUTIVE bytes that may be supplied */
int
ring_empty_consecutive(Ring *ring)
{
    if ((ring->consume < ring->supply) || ring_empty(ring)) {
			    /*
			     * if consume is "below" supply, or empty, then
			     * return distance to the top
			     */
	return ring_subtract(ring, ring->top, ring->supply);
    } else {
				    /*
				     * else, return what we may.
				     */
	return ring_subtract(ring, ring->consume, ring->supply);
    }
}

/* Return the number of bytes that are available for consuming
 * (but don't give more than enough to get to cross over set mark)
 */

int
ring_full_count(Ring *ring)
{
    if ((ring->mark == NULL) || (ring->mark == ring->consume)) {
	if (ring_full(ring)) {
	    return ring->size;	/* nothing consumed, but full */
	} else {
	    return ring_subtract(ring, ring->supply, ring->consume);
	}
    } else {
	return ring_subtract(ring, ring->mark, ring->consume);
    }
}

/*
 * Return the number of CONSECUTIVE bytes available for consuming.
 * However, don't return more than enough to cross over set mark.
 */
int
ring_full_consecutive(Ring *ring)
{
    if ((ring->mark == NULL) || (ring->mark == ring->consume)) {
	if ((ring->supply < ring->consume) || ring_full(ring)) {
	    return ring_subtract(ring, ring->top, ring->consume);
	} else {
	    return ring_subtract(ring, ring->supply, ring->consume);
	}
    } else {
	if (ring->mark < ring->consume) {
	    return ring_subtract(ring, ring->top, ring->consume);
	} else {	/* Else, distance to mark */
	    return ring_subtract(ring, ring->mark, ring->consume);
	}
    }
}

/*
 * Move data into the "supply" portion of of the ring buffer.
 */
void
ring_supply_data(Ring *ring, unsigned char *buffer, int count)
{
    int i;

    while (count) {
	i = ring_empty_consecutive(ring);
	if (i > count)
		i = count;
	memcpy(ring->supply, buffer, i);
	ring_supplied(ring, i);
	count -= i;
	buffer += i;
    }
}
