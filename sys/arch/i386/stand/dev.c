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
#include <stand.h>

int biosstrategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int biosopen __P((struct open_file *, ...));
int biosclose __P((struct open_file *));
int biosioctl __P((struct open_file *, u_long, void *));

struct devsw	devsw[] = {
	{ "BIOS", biosstrategy, biosopen, biosclose, biosioctl },
	{ NULL, NULL, NULL, NULL, NULL }
};

int	ndevsw = sizeof(devsw)/sizeof(devsw[0]);

void
putchar(c)
	int	c;
{
	extern int putc(int);

	if (c == '\n')
		putc('\r');
	putc(c);
}

int                                                                 
getchar()                                                                
{
	extern int getc();

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
	extern int ischar();
	extern void usleep(int);
	while (n-- && !ischar())
		usleep(10);
}


/* pass dev_t to the open routines */
int
devopen(struct open_file *f, const char *fname, char **file)
{
	return 0;
}
