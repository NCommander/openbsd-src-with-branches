/*

mpaux.c

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Sun Jul 16 04:29:30 1995 ylo

This file contains various auxiliary functions related to multiple
precision integers.

*/

#include "includes.h"
RCSID("$Id: mpaux.c,v 1.3 1999/05/04 11:58:51 bg Exp $");

#include <ssl/bn.h>
#include "getput.h"
#include "xmalloc.h"
#include "ssh_md5.h"

void
compute_session_id(unsigned char session_id[16],
		   unsigned char cookie[8],
		   unsigned int host_key_bits,
		   BIGNUM *host_key_n,
		   unsigned int session_key_bits,
		   BIGNUM *session_key_n)
{
  unsigned int bytes = (host_key_bits + 7) / 8 + (session_key_bits + 7) / 8 + 8;
  unsigned char *buf = xmalloc(bytes);
  struct MD5Context md;
  
  BN_bn2bin(host_key_n, buf);
  BN_bn2bin(session_key_n, buf + (host_key_bits + 7 ) / 8);
  memcpy(buf + (host_key_bits + 7) / 8 + (session_key_bits + 7) / 8,
	 cookie, 8);
  MD5Init(&md);
  MD5Update(&md, buf, bytes);
  MD5Final(session_id, &md);
  xfree(buf);
}
