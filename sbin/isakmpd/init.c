/*	$OpenBSD: init.c,v 1.9 1999/07/17 21:54:39 niklas Exp $	*/
/*	$EOM: init.c,v 1.18 1999/08/26 11:21:49 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

/* XXX This file could easily be built dynamically instead.  */

#include "sysdep.h"

#include "app.h"
#include "cert.h"
#include "conf.h"
#include "connection.h"
#include "cookie.h"
#include "doi.h"
#include "exchange.h"
#include "init.h"
#include "ipsec.h"
#include "isakmp_doi.h"
#include "libcrypto.h"
#include "math_group.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "udp.h"
#include "ui.h"

#if defined (USE_KEYNOTE) || defined (HAVE_DLOPEN)
#include "policy.h"
#endif

void
init ()
{
  app_init ();
  doi_init ();
  exchange_init ();
  group_init ();
  ipsec_init ();
  isakmp_doi_init ();
  libcrypto_init ();
  timer_init ();

  /* The following group are depending on timer_init having run.  */
  conf_init ();
  connection_init ();
  cookie_init ();

  /* Depends on conf_init having run */
  cert_init ();

  sa_init ();
  transport_init ();
  udp_init ();
  ui_init ();

#if defined (USE_KEYNOTE) || defined (HAVE_DLOPEN)
  /* policy_init depends on conf_init having run.  */
  policy_init ();
#endif
}
