/*	$OpenBSD: ttydef.h,v 1.4 2001/01/29 01:58:10 niklas Exp $	*/

#ifndef TTYDEF_H
#define TTYDEF_H

/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

#define GOSLING				/* Compile in fancy display.	 */
/* #define	MEMMAP		      *//* Not memory mapped video.	 */

/* #define	MOVE_STANDOUT	      *//* don't move in standout mode	 */
#define STANDOUT_GLITCH			/* possible standout glitch	 */
#define TERMCAP				/* for possible use in ttyio.c	 */

#define getkbd()	(ttgetc())

#ifndef XKEYS
#define ttykeymapinit() {}
#endif

#define	putpad(str, num)	tputs(str, num, ttputc)

#define	KFIRST	K00
#define	KLAST	K00

#endif /* TTYDEF_H */
