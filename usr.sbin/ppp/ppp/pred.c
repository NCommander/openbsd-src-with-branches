/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
 *                    Ian Donaldson <iand@labtam.labtam.oz.au>
 *                    Carsten Bormann <cabo@cs.tu-berlin.de>
 *                    Dave Rand <dlr@bungi.com>/<dave_rand@novell.com>
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
 *	$Id: pred.c,v 1.22 1998/08/07 18:42:50 brian Exp $
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "pred.h"

/* The following hash code is the heart of the algorithm:
 * It builds a sliding hash sum of the previous 3-and-a-bit characters
 * which will be used to index the guess table.
 * A better hash function would result in additional compression,
 * at the expense of time.
 */
#define HASH(state, x) state->hash = (state->hash << 4) ^ (x)
#define GUESS_TABLE_SIZE 65536

struct pred1_state {
  u_short hash;
  u_char dict[GUESS_TABLE_SIZE];
};

static int
compress(struct pred1_state *state, u_char *source, u_char *dest, int len)
{
  int i, bitmask;
  unsigned char *flagdest, flags, *orgdest;

  orgdest = dest;
  while (len) {
    flagdest = dest++;
    flags = 0;			/* All guess wrong initially */
    for (bitmask = 1, i = 0; i < 8 && len; i++, bitmask <<= 1) {
      if (state->dict[state->hash] == *source) {
	flags |= bitmask;	/* Guess was right - don't output */
      } else {
	state->dict[state->hash] = *source;
	*dest++ = *source;	/* Guess wrong, output char */
      }
      HASH(state, *source++);
      len--;
    }
    *flagdest = flags;
  }
  return (dest - orgdest);
}

static void
SyncTable(struct pred1_state *state, u_char * source, u_char * dest, int len)
{
  while (len--) {
    if (state->dict[state->hash] != *source)
      state->dict[state->hash] = *source;
    HASH(state, *dest++ = *source++);
  }
}

static int
decompress(struct pred1_state *state, u_char * source, u_char * dest, int len)
{
  int i, bitmask;
  unsigned char flags, *orgdest;

  orgdest = dest;
  while (len) {
    flags = *source++;
    len--;
    for (i = 0, bitmask = 1; i < 8; i++, bitmask <<= 1) {
      if (flags & bitmask) {
	*dest = state->dict[state->hash];	/* Guess correct */
      } else {
	if (!len)
	  break;		/* we seem to be really done -- cabo */
	state->dict[state->hash] = *source;	/* Guess wrong */
	*dest = *source++;	/* Read from source */
	len--;
      }
      HASH(state, *dest++);
    }
  }
  return (dest - orgdest);
}

static void
Pred1Term(void *v)
{
  struct pred1_state *state = (struct pred1_state *)v;
  free(state);
}

static void
Pred1ResetInput(void *v)
{
  struct pred1_state *state = (struct pred1_state *)v;
  state->hash = 0;
  memset(state->dict, '\0', sizeof state->dict);
  log_Printf(LogCCP, "Predictor1: Input channel reset\n");
}

static void
Pred1ResetOutput(void *v)
{
  struct pred1_state *state = (struct pred1_state *)v;
  state->hash = 0;
  memset(state->dict, '\0', sizeof state->dict);
  log_Printf(LogCCP, "Predictor1: Output channel reset\n");
}

static void *
Pred1InitInput(struct lcp_opt *o)
{
  struct pred1_state *state;
  state = (struct pred1_state *)malloc(sizeof(struct pred1_state));
  if (state != NULL)
    Pred1ResetInput(state);
  return state;
}

static void *
Pred1InitOutput(struct lcp_opt *o)
{
  struct pred1_state *state;
  state = (struct pred1_state *)malloc(sizeof(struct pred1_state));
  if (state != NULL)
    Pred1ResetOutput(state);
  return state;
}

static int
Pred1Output(void *v, struct ccp *ccp, struct link *l, int pri, u_short proto,
            struct mbuf *bp)
{
  struct pred1_state *state = (struct pred1_state *)v;
  struct mbuf *mwp;
  u_char *cp, *wp, *hp;
  int orglen, len;
  u_char bufp[MAX_MTU + 2];
  u_short fcs;

  orglen = mbuf_Length(bp) + 2;	/* add count of proto */
  mwp = mbuf_Alloc((orglen + 2) / 8 * 9 + 12, MB_HDLCOUT);
  hp = wp = MBUF_CTOP(mwp);
  cp = bufp;
  *wp++ = *cp++ = orglen >> 8;
  *wp++ = *cp++ = orglen & 0377;
  *cp++ = proto >> 8;
  *cp++ = proto & 0377;
  mbuf_Read(bp, cp, orglen - 2);
  fcs = hdlc_Fcs(INITFCS, bufp, 2 + orglen);
  fcs = ~fcs;

  len = compress(state, bufp + 2, wp, orglen);
  log_Printf(LogDEBUG, "Pred1Output: orglen (%d) --> len (%d)\n", orglen, len);
  ccp->uncompout += orglen;
  if (len < orglen) {
    *hp |= 0x80;
    wp += len;
    ccp->compout += len;
  } else {
    memcpy(wp, bufp + 2, orglen);
    wp += orglen;
    ccp->compout += orglen;
  }

  *wp++ = fcs & 0377;
  *wp++ = fcs >> 8;
  mwp->cnt = wp - MBUF_CTOP(mwp);
  hdlc_Output(l, PRI_NORMAL, ccp_Proto(ccp), mwp);
  return 1;
}

static struct mbuf *
Pred1Input(void *v, struct ccp *ccp, u_short *proto, struct mbuf *bp)
{
  struct pred1_state *state = (struct pred1_state *)v;
  u_char *cp, *pp;
  int len, olen, len1;
  struct mbuf *wp;
  u_char *bufp;
  u_short fcs;

  wp = mbuf_Alloc(MAX_MTU + 2, MB_IPIN);
  cp = MBUF_CTOP(bp);
  olen = mbuf_Length(bp);
  pp = bufp = MBUF_CTOP(wp);
  *pp++ = *cp & 0177;
  len = *cp++ << 8;
  *pp++ = *cp;
  len += *cp++;
  ccp->uncompin += len & 0x7fff;
  if (len & 0x8000) {
    len1 = decompress(state, cp, pp, olen - 4);
    ccp->compin += olen;
    len &= 0x7fff;
    if (len != len1) {		/* Error is detected. Send reset request */
      log_Printf(LogCCP, "Pred1: Length error\n");
      ccp_SendResetReq(&ccp->fsm);
      mbuf_Free(bp);
      mbuf_Free(wp);
      return NULL;
    }
    cp += olen - 4;
    pp += len1;
  } else {
    ccp->compin += len;
    SyncTable(state, cp, pp, len);
    cp += len;
    pp += len;
  }
  *pp++ = *cp++;		/* CRC */
  *pp++ = *cp++;
  fcs = hdlc_Fcs(INITFCS, bufp, wp->cnt = pp - bufp);
  if (fcs != GOODFCS)
    log_Printf(LogDEBUG, "Pred1Input: fcs = 0x%04x (%s), len = 0x%x,"
	      " olen = 0x%x\n", fcs, (fcs == GOODFCS) ? "good" : "bad",
	      len, olen);
  if (fcs == GOODFCS) {
    wp->offset += 2;		/* skip length */
    wp->cnt -= 4;		/* skip length & CRC */
    pp = MBUF_CTOP(wp);
    *proto = *pp++;
    if (*proto & 1) {
      wp->offset++;
      wp->cnt--;
    } else {
      wp->offset += 2;
      wp->cnt -= 2;
      *proto = (*proto << 8) | *pp++;
    }
    mbuf_Free(bp);
    return wp;
  } else {
    log_DumpBp(LogHDLC, "Bad FCS", wp);
    ccp_SendResetReq(&ccp->fsm);
    mbuf_Free(wp);
  }
  mbuf_Free(bp);
  return NULL;
}

static void
Pred1DictSetup(void *v, struct ccp *ccp, u_short proto, struct mbuf * bp)
{
}

static const char *
Pred1DispOpts(struct lcp_opt *o)
{
  return NULL;
}

static void
Pred1InitOptsOutput(struct lcp_opt *o, const struct ccp_config *cfg)
{
  o->len = 2;
}

static int
Pred1SetOptsOutput(struct lcp_opt *o)
{
  if (o->len != 2) {
    o->len = 2;
    return MODE_NAK;
  }
  return MODE_ACK;
}

static int
Pred1SetOptsInput(struct lcp_opt *o, const struct ccp_config *cfg)
{
  return Pred1SetOptsOutput(o);
}

const struct ccp_algorithm Pred1Algorithm = {
  TY_PRED1,
  CCP_NEG_PRED1,
  Pred1DispOpts,
  {
    Pred1SetOptsInput,
    Pred1InitInput,
    Pred1Term,
    Pred1ResetInput,
    Pred1Input,
    Pred1DictSetup
  },
  {
    Pred1InitOptsOutput,
    Pred1SetOptsOutput,
    Pred1InitOutput,
    Pred1Term,
    Pred1ResetOutput,
    Pred1Output
  },
};
