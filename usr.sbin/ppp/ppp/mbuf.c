/*
 *	      PPP Memory handling module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: mbuf.c,v 1.22 1998/08/25 17:48:42 brian Exp $
 *
 */
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>

#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "descriptor.h"
#include "prompt.h"
#include "main.h"

static struct memmap {
  struct mbuf *queue;
  int fragments, octets;
} MemMap[MB_MAX + 2];

static int totalalloced;

int
mbuf_Length(struct mbuf * bp)
{
  int len;

  for (len = 0; bp; bp = bp->next)
    len += bp->cnt;
  return (len);
}

struct mbuf *
mbuf_Alloc(int cnt, int type)
{
  struct mbuf *bp;

  if (type > MB_MAX)
    log_Printf(LogERROR, "Bad mbuf type %d\n", type);
  bp = malloc(sizeof(struct mbuf) + cnt);
  if (bp == NULL) {
    log_Printf(LogALERT, "failed to allocate memory: %ld\n",
               (long)sizeof(struct mbuf));
    AbortProgram(EX_OSERR);
  }
  memset(bp, '\0', sizeof(struct mbuf));
  MemMap[type].fragments++;
  MemMap[type].octets += cnt;
  totalalloced += cnt;
  bp->size = bp->cnt = cnt;
  bp->type = type;
  bp->pnext = NULL;
  return (bp);
}

struct mbuf *
mbuf_FreeSeg(struct mbuf * bp)
{
  struct mbuf *nbp;

  if (bp) {
    nbp = bp->next;
    MemMap[bp->type].fragments--;
    MemMap[bp->type].octets -= bp->size;
    totalalloced -= bp->size;
    free(bp);
    return (nbp);
  }
  return (bp);
}

void
mbuf_Free(struct mbuf * bp)
{
  while (bp)
    bp = mbuf_FreeSeg(bp);
}

struct mbuf *
mbuf_Read(struct mbuf * bp, u_char * ptr, int len)
{
  int nb;

  while (bp && len > 0) {
    if (len > bp->cnt)
      nb = bp->cnt;
    else
      nb = len;
    memcpy(ptr, MBUF_CTOP(bp), nb);
    ptr += nb;
    bp->cnt -= nb;
    len -= nb;
    bp->offset += nb;
    if (bp->cnt == 0) {
#ifdef notdef
      bp = bp->next;
#else
      bp = mbuf_FreeSeg(bp);
#endif
    }
  }
  return (bp);
}

void
mbuf_Write(struct mbuf * bp, u_char * ptr, int cnt)
{
  int plen;
  int nb;

  plen = mbuf_Length(bp);
  if (plen < cnt)
    cnt = plen;

  while (cnt > 0) {
    nb = (cnt < bp->cnt) ? cnt : bp->cnt;
    memcpy(MBUF_CTOP(bp), ptr, nb);
    cnt -= bp->cnt;
    bp = bp->next;
  }
}

int
mbuf_Show(struct cmdargs const *arg)
{
  int i;
  static const char *mbuftype[] = { 
    "async", "fsm", "cbcp", "hdlcout", "ipin", "echo", "lqr", "link",
    "vjcomp", "ipq", "mp" };

  prompt_Printf(arg->prompt, "Fragments (octets) in use:\n");
  for (i = 1; i < MB_MAX; i += 2)
    prompt_Printf(arg->prompt, "%10.10s: %04d (%06d)\t%10.10s: %04d (%06d)\n",
	    mbuftype[i-1], MemMap[i].fragments, MemMap[i].octets, mbuftype[i],
            MemMap[i+1].fragments, MemMap[i+1].octets);

  if (i == MB_MAX)
    prompt_Printf(arg->prompt, "%10.10s: %04d (%06d)\n",
                  mbuftype[i-1], MemMap[i].fragments, MemMap[i].octets);

  return 0;
}

void
mbuf_Log()
{
  log_Printf(LogDEBUG, "mbuf_Log: mem alloced: %d\n", totalalloced);
  log_Printf(LogDEBUG, "mbuf_Log:  1: %d  2: %d   3: %d   4: %d\n",
	MemMap[1].octets, MemMap[2].octets, MemMap[3].octets, MemMap[4].octets);
  log_Printf(LogDEBUG, "mbuf_Log:  5: %d  6: %d   7: %d   8: %d\n",
	MemMap[5].octets, MemMap[6].octets, MemMap[7].octets, MemMap[8].octets);
  log_Printf(LogDEBUG, "mbuf_Log:  9: %d 10: %d  11: %d\n",
	MemMap[9].octets, MemMap[10].octets, MemMap[11].octets);
}

struct mbuf *
mbuf_Dequeue(struct mqueue *q)
{
  struct mbuf *bp;
  
  log_Printf(LogDEBUG, "mbuf_Dequeue: queue len = %d\n", q->qlen);
  bp = q->top;
  if (bp) {
    q->top = q->top->pnext;
    q->qlen--;
    if (q->top == NULL) {
      q->last = q->top;
      if (q->qlen)
	log_Printf(LogERROR, "mbuf_Dequeue: Not zero (%d)!!!\n", q->qlen);
    }
    bp->pnext = NULL;
  }

  return bp;
}


void
mbuf_Enqueue(struct mqueue *queue, struct mbuf *bp)
{
  if (queue->last) {
    queue->last->pnext = bp;
    queue->last = bp;
  } else
    queue->last = queue->top = bp;
  queue->qlen++;
  log_Printf(LogDEBUG, "mbuf_Enqueue: len = %d\n", queue->qlen);
}
