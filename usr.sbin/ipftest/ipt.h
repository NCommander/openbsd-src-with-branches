/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 * $Id: ipt.h,v 1.1.1.1 1996/06/23 07:27:16 darrenr Exp $
 */

#include <fcntl.h>

struct	ipread	{
	int	(*r_open)();
	int	(*r_close)();
	int	(*r_readip)();
};
