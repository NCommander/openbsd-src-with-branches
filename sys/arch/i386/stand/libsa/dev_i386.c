/*	$OpenBSD$	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <libsa.h>
#include "biosdev.h"

/* pass dev_t to the open routines */
int
devopen(struct open_file *f, const char *fname, char **file)
{
	struct devsw *dp;
	char	*cp = (char *)fname;

	/* search for device specification */
	while (*cp != 0 && *cp != '(')
		cp++;
	if (*cp == 0)	/* no dev */
		cp = (char *)fname;
	else {
		if (*cp++ == '(') {
			char **devp = (char **)devs;

			for (; *devp != NULL && 
				(fname[0] != (*devp)[0] ||
				 fname[1] != (*devp)[1]); devp++)
				;
			if (*devp == NULL) {
				printf("Unknown device");
				errno = ENXIO;
				return -1;
			}
			maj = devp - (char **)devs;
		}
		/* check syntax */
		if (cp[1] != ',' || cp[3] != ')') {
			printf("Syntax error\n");
			errno = EINVAL;
			return -1;
		}

		/* get unit */
		if ('0' <= *cp && *cp <= '9')
			unit = *cp++ - '0';
		else {
			printf("Bad unit number\n");
			errno = ENXIO;
			return -1;
		}
		cp++;	/* skip ',' */
		/* get partition */
		if ('a' <= *cp && *cp <= 'p')
			part = *cp++ - 'a';
		else {
			printf("Bad partition id\n");
			errno = ENXIO;
			return -1;
		}
		cp++;	/* skip ')' */
		if (*cp == 0) {
			errno = EINVAL;
			return -1;
		}

	}

	switch (maj) {
	case 0:
	case 4:
		bootdev = unit | 0x80;
		break;
	case 1:
		bootdev = unit | 0x80;
		unit = 0;
		break;
	case 2:
		bootdev = unit;
		break;
	case 3:
#ifdef DEBUG
		printf("Wangtek is unsupported\n");
#endif
		errno = ENXIO;
		return -1;
	default:
		break;
	}

	*file = (char *)fname;
	f->f_dev = dp = &devsw[0];

	return (*dp->dv_open)(f, *file);
}

void
putchar(c)
	int	c;
{
	if (c == '\n')
		putc('\r');
	putc(c);
}

#ifndef	STRIPPED
int
getchar()
{
	int c = getc();

	if (c == '\b' || c == '\177')
		return(c);

	if (c == '\r')
		c = '\n';

	putchar(c);

	return(c);
}

void
wait(n)
	int	n;
{
	while (n-- && !ischar())
		usleep(10);
}
#endif
