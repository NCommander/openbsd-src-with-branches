/*	$OpenBSD: bootparam.h,v 1.2 1996/09/23 14:18:49 mickey Exp $	*/

int bp_whoami __P((int sock));
int bp_getfile __P((int sock, char *key, struct in_addr *addrp, char *path));

