/*	$OpenBSD: key.h,v 1.3 2001/01/29 01:58:07 niklas Exp $	*/

/* key.h: Insert file for mg 2 functions that need to reference key pressed */

#define MAXKEY	8			/* maximum number of prefix chars */

struct key {				/* the chacter sequence in a key */
	int	k_count;		/* number of chars */
	KCHAR	k_chars[MAXKEY];	/* chars */
};

extern struct key key;
