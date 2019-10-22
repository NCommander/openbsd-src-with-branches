/*	$OpenBSD: ntohl.S,v 1.3 2015/05/29 09:25:28 uebayasi Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(ntohl)
	RETGUARD_SETUP(ntohl, r11)
	movl    %edi,%eax
	bswapl  %eax
	RETGUARD_CHECK(ntohl, r11)
	ret
END(ntohl)
