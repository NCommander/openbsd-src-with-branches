/*	$Id$	*/

/*
 * Copyright (c) 2002 by Thorsten "mirabile" Glaser <x86@ePOST.de>
 *
 * Permission is hereby granted to any person obtaining a copy of this work
 * to deal in the work, without restrictions, including unlimited rights to
 * use, copy, modify, merge, publish, distribute, sublicense or sell copies
 * of the work, and to permit persons to whom the work is furnished to also
 * do so, as long as due credit is given to the original author and contri-
 * butors, and the following disclaimer is kept in all substantial portions
 * of the work or accompanying documentation:
 *
 * This work is provided "AS IS", without warranty of any kind, neither ex-
 * press nor implied, including, but not limited to, the warranties of mer-
 * chantability, fitness for particular purposes and noninfringement. In NO
 * event shall the author and contributors be liable for any claim, damages
 * and such, whether in contract, strict liability or otherwise, arising in
 * any way out of this work, even if advised of the possibility of such.
 */

/* Leap second support for NTP clients (generic) */

static const char RCSId[] = "$OpenBSD$";


/* I could include tzfile.h, but this would make the code unportable
 * at no real benefit. Read tzfile.h for why.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "ntpleaps.h"

u_int64_t *leapsecs = NULL;
unsigned int leapsecs_num = 0;
static int flaginit = 0;
static int flagwarn = 0;


int
ntpleaps_init(void)
{
	if (flaginit)
		return 0;

	if (!ntpleaps_read()) {
		flaginit = 1;
		return(0);
	}

	/* This does not really hurt, but users will complain about
	 * off-by-22-seconds (at time of coding) errors if we don't warn.
	 */
	if(!flagwarn) {
		fputs("Warning: error reading tzfile. You will NOT be\n"
		    "able to get legal time or posix compliance!\n", stderr);
		flagwarn = 1;	/* put it only once */
	}

	return (-1);
}

int
ntpleaps_sub(u_int64_t *t)
{
	unsigned int i = 0;
	u_int64_t u;
	int r=1;

	if (ntpleaps_init() == -1)
		return(-1);

	u = *t;

	while (i<leapsecs_num) {
		if (u < leapsecs[i])
			break;
		if (u == leapsecs[i++])
			goto do_sub;
	}
	--r;

do_sub:
	*t=u-i;
	return (r);
}

int
ntpleaps_read(void)
{
	int fd;
	unsigned int r;
	u_int8_t buf[32];
	u_int32_t m1,m2,m3;
	u_int64_t s;
	u_int64_t *l;

	fd = open("/usr/share/zoneinfo/right/UTC", O_RDONLY | O_NDELAY);
	if (fd == -1)
		return (-1);

	/* Check signature */
	read(fd, buf, 4); buf[4]=0;
	if(strcmp((const char *)buf, "TZif")) {
		close(fd);
		return (-1);
	}

	/* Pre-initalize buf[24..27] so we need not check read(2) result */
	buf[24] = 0;
	buf[25] = 0;
	buf[26] = 0;
	buf[27] = 0;

	/* Skip uninteresting parts of header */
	read(fd, buf, 28);

	/* Read number of leap second entries */
	r=ntohl( *((u_int32_t *) (buf + 24)) );
	/* Check for plausibility - arbitrary values */
	if( (r<20) || (r>60000) ) {
		close(fd);
		return (-1);
	}
	if(( l = (u_int64_t *) malloc(r << 3)) == NULL) {
		close(fd);
		return (-1);
	}

	/* Skip further uninteresting stuff */
	read(fd, buf, 12);
	m1 = ntohl( *((u_int32_t *)(buf)) );
	m2 = ntohl( *((u_int32_t *)(buf+4)) );
	m3 = ntohl( *((u_int32_t *)(buf+8)) );
	m3 += (m1 << 2)+m1+(m2 << 2)+(m2 << 1);
	lseek(fd, (off_t)m3, SEEK_CUR);

	/* Now go parse the tzfile leap second info */
	for (m1=0; m1<r; m1++) {
		read(fd, buf, 8);
		m2 = ntohl( *((u_int32_t *)buf) );
		s = NTPLEAPS_OFFSET + (u_int64_t) m2;
	/* Assume just _one_ leap second on each entry, and compensate
	 * the lacking error checking by validating the first entry
	 * against the known value */
		if(!m1 && s != 0x4000000004B2580AULL)
			return (-1);
		l[m1] = s;
	}

	/* Clean up and activate the table */
	close(fd);
	if (leapsecs != NULL)
		free(leapsecs);
	leapsecs = l;
	leapsecs_num = r;

	return (0);
}
