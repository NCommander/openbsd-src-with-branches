/*-
 * Copyright (c) 1999 Brian Somers <brian@Awfulhak.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id:$
 */

#include <sys/types.h>

#include <stdio.h>
#include <termios.h>

#include "layer.h"
#include "acf.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "mbuf.h"
#include "proto.h"
#include "lcp.h"
#include "throughput.h"
#include "lqr.h"
#include "hdlc.h"
#include "ccp.h"
#include "link.h"

int
proto_WrapperOctets(struct lcp *lcp, u_short proto)
{
  return (lcp->his_protocomp && !(proto & 0xff00)) ? 1 : 2;
}

struct mbuf *
proto_Prepend(struct mbuf *bp, u_short proto, unsigned comp, int extra)
{
  u_char cp[2];

  cp[0] = proto >> 8;
  cp[1] = proto & 0xff;

  if (comp && cp[0] == 0)
    bp = mbuf_Prepend(bp, cp + 1, 1, extra);
  else
    bp = mbuf_Prepend(bp, cp, 2, extra);

  return bp;
}

static struct mbuf *
proto_LayerPush(struct bundle *b, struct link *l, struct mbuf *bp,
                int pri, u_short *proto)
{
  bp = proto_Prepend(bp, *proto, l->lcp.his_protocomp,
                     acf_WrapperOctets(&l->lcp, *proto));
  link_ProtocolRecord(l, *proto, PROTO_OUT);

  return bp;
}

static struct mbuf *
proto_LayerPull(struct bundle *b, struct link *l, struct mbuf *bp,
                u_short *proto)
{
  u_char cp[2];
  size_t got;

  if ((got = mbuf_View(bp, cp, 2)) == 0) {
    mbuf_Free(bp);
    return NULL;
  }

  *proto = cp[0];
  if (!(*proto & 1)) {
    if (got == 1) {
      mbuf_Free(bp);
      return NULL;
    }
    bp = mbuf_Read(bp, cp, 2);
    *proto = (*proto << 8) | cp[1];
  } else
    bp = mbuf_Read(bp, cp, 1);

  link_ProtocolRecord(l, *proto, PROTO_IN);

  return bp;
}

struct layer protolayer =
  { LAYER_PROTO, "proto", proto_LayerPush, proto_LayerPull };
