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
 * $Id: mbuf.c,v 1.8 1999/08/26 06:39:19 brian Exp $
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

#define BUCKET_CHUNK	20
#define BUCKET_HASH	256

struct mbucket;

struct mfree {
  struct mbucket *next;
  size_t count;
};

static struct mbucket {
  union {
    struct mbuf m;
    struct mfree f;
  } u;
} *bucket[(M_MAXLEN + sizeof(struct mbuf)) / BUCKET_HASH];

#define M_BINDEX(sz)	(((sz) + sizeof(struct mbuf) - 1) / BUCKET_HASH)
#define M_BUCKET(sz)	(bucket + M_BINDEX(sz))
#define M_ROUNDUP(sz)	((M_BINDEX(sz) + 1) * BUCKET_HASH)

static struct memmap {
  struct mbuf *queue;
  size_t fragments;
  size_t octets;
} MemMap[MB_MAX + 1];

static unsigned long long mbuf_Mallocs, mbuf_Frees;

int
m_length(struct mbuf *bp)
{
  int len;

  for (len = 0; bp; bp = bp->m_next)
    len += bp->m_len;
  return len;
}

struct mbuf *
m_get(size_t m_len, int type)
{
  struct mbucket **mb;
  struct mbuf *bp;
  size_t size;

  if (type > MB_MAX) {
    log_Printf(LogERROR, "Bad mbuf type %d\n", type);
    type = MB_UNKNOWN;
  }
  
  if (m_len > M_MAXLEN || m_len == 0) {
    log_Printf(LogERROR, "Request for mbuf size %lu denied\n", (u_long)m_len);
    AbortProgram(EX_OSERR);
  }

  mb = M_BUCKET(m_len);
  size = M_ROUNDUP(m_len);

  if (*mb) {
    /* We've got some free blocks of the right size */
    bp = &(*mb)->u.m;
    if (--(*mb)->u.f.count == 0)
      *mb = (*mb)->u.f.next;
    else {
      ((struct mbucket *)((char *)*mb + size))->u.f.count = (*mb)->u.f.count;
      *mb = (struct mbucket *)((char *)*mb + size);
      (*mb)->u.f.next = NULL;
    }
  } else {
    /*
     * Allocate another chunk of mbufs, use the first and put the rest on
     * the free list
     */
    *mb = (struct mbucket *)malloc(BUCKET_CHUNK * size);
    if (*mb == NULL) {
      log_Printf(LogALERT, "Failed to allocate memory (%u)\n",
                 BUCKET_CHUNK * size);
      AbortProgram(EX_OSERR);
    }
    bp = &(*mb)->u.m;
    *mb = (struct mbucket *)((char *)*mb + size);
    (*mb)->u.f.count = BUCKET_CHUNK - 1;
    (*mb)->u.f.next = NULL;
  }

  mbuf_Mallocs++;

  memset(bp, '\0', sizeof(struct mbuf));
  bp->m_size = size - sizeof *bp;
  bp->m_len = m_len;
  bp->m_type = type;

  MemMap[type].fragments++;
  MemMap[type].octets += bp->m_size;

  return bp;
}

struct mbuf *
m_free(struct mbuf *bp)
{
  struct mbucket **mb, *f;
  struct mbuf *nbp;

  if ((f = (struct mbucket *)bp) != NULL) {
    MemMap[bp->m_type].fragments--;
    MemMap[bp->m_type].octets -= bp->m_size;

    nbp = bp->m_next;
    mb = M_BUCKET(bp->m_size);
    f->u.f.next = *mb;
    f->u.f.count = 1;
    *mb = f;

    mbuf_Frees++;
    bp = nbp;
  }

  return bp;
}

void
m_freem(struct mbuf *bp)
{
  while (bp)
    bp = m_free(bp);
}

struct mbuf *
mbuf_Read(struct mbuf *bp, void *v, size_t len)
{
  int nb;
  u_char *ptr = v;

  while (bp && len > 0) {
    if (len > bp->m_len)
      nb = bp->m_len;
    else
      nb = len;
    if (nb) {
      memcpy(ptr, MBUF_CTOP(bp), nb);
      ptr += nb;
      bp->m_len -= nb;
      len -= nb;
      bp->m_offset += nb;
    }
    if (bp->m_len == 0)
      bp = m_free(bp);
  }

  while (bp && bp->m_len == 0)
    bp = m_free(bp);

  return bp;
}

size_t
mbuf_View(struct mbuf *bp, void *v, size_t len)
{
  size_t nb, l = len;
  u_char *ptr = v;

  while (bp && l > 0) {
    if (l > bp->m_len)
      nb = bp->m_len;
    else
      nb = l;
    memcpy(ptr, MBUF_CTOP(bp), nb);
    ptr += nb;
    l -= nb;
    bp = bp->m_next;
  }

  return len - l;
}

struct mbuf *
m_prepend(struct mbuf *bp, const void *ptr, size_t len, size_t extra)
{
  struct mbuf *head;

  if (bp && bp->m_offset) {
    if (bp->m_offset >= len) {
      bp->m_offset -= len;
      bp->m_len += len;
      memcpy(MBUF_CTOP(bp), ptr, len);
      return bp;
    }
    len -= bp->m_offset;
    memcpy(bp + 1, (const char *)ptr + len, bp->m_offset);
    bp->m_len += bp->m_offset;
    bp->m_offset = 0;
  }

  head = m_get(len + extra, bp ? bp->m_type : MB_UNKNOWN);
  head->m_offset = extra;
  head->m_len -= extra;
  if (ptr)
    memcpy(MBUF_CTOP(head), ptr, len);
  head->m_next = bp;

  return head;
}

struct mbuf *
m_adj(struct mbuf *bp, ssize_t n)
{
  if (n > 0) {
    while (bp) {
      if (n < bp->m_len) {
        bp->m_len = n;
        bp->m_offset += n;
        return bp;
      }
      n -= bp->m_len;
      bp = m_free(bp);
    }
  } else {
    if ((n = m_length(bp) + n) <= 0) {
      m_freem(bp);
      return NULL;
    }
    for (; bp; bp = bp->m_next, n -= bp->m_len)
      if (n < bp->m_len) {
        bp->m_len = n;
        m_freem(bp->m_next);
        bp->m_next = NULL;
        break;
      }
  }

  return bp;
}

void
mbuf_Write(struct mbuf *bp, const void *ptr, size_t m_len)
{
  int plen;
  int nb;

  plen = m_length(bp);
  if (plen < m_len)
    m_len = plen;

  while (m_len > 0) {
    nb = (m_len < bp->m_len) ? m_len : bp->m_len;
    memcpy(MBUF_CTOP(bp), ptr, nb);
    m_len -= bp->m_len;
    bp = bp->m_next;
  }
}

int
mbuf_Show(struct cmdargs const *arg)
{
  int i;
  static const char * const mbuftype[] = { 
    "ip in", "ip out", "nat in", "nat out", "mp in", "mp out",
    "vj in", "vj out", "icompd in", "icompd out", "compd in", "compd out",
    "lqr in", "lqr out", "echo in", "echo out", "proto in", "proto out",
    "acf in", "acf out", "sync in", "sync out", "hdlc in", "hdlc out",
    "async in", "async out", "cbcp in", "cbcp out", "chap in", "chap out",
    "pap in", "pap out", "ccp in", "ccp out", "ipcp in", "ipcp out",
    "lcp in", "lcp out", "unknown"
  };

  prompt_Printf(arg->prompt, "Fragments (octets) in use:\n");
  for (i = 0; i < MB_MAX; i += 2)
    prompt_Printf(arg->prompt, "%10.10s: %04lu (%06lu)\t"
                  "%10.10s: %04lu (%06lu)\n",
	          mbuftype[i], (u_long)MemMap[i].fragments,
                  (u_long)MemMap[i].octets, mbuftype[i+1],
                  (u_long)MemMap[i+1].fragments, (u_long)MemMap[i+1].octets);

  if (i == MB_MAX)
    prompt_Printf(arg->prompt, "%10.10s: %04lu (%06lu)\n",
                  mbuftype[i], (u_long)MemMap[i].fragments,
                  (u_long)MemMap[i].octets);

  prompt_Printf(arg->prompt, "Mallocs: %llu,   Frees: %llu\n",
                mbuf_Mallocs, mbuf_Frees);

  return 0;
}

struct mbuf *
m_dequeue(struct mqueue *q)
{
  struct mbuf *bp;
  
  log_Printf(LogDEBUG, "m_dequeue: queue len = %lu\n", (u_long)q->len);
  bp = q->top;
  if (bp) {
    q->top = q->top->m_nextpkt;
    q->len--;
    if (q->top == NULL) {
      q->last = q->top;
      if (q->len)
	log_Printf(LogERROR, "m_dequeue: Not zero (%lu)!!!\n",
                   (u_long)q->len);
    }
    bp->m_nextpkt = NULL;
  }

  return bp;
}

void
m_enqueue(struct mqueue *queue, struct mbuf *bp)
{
  if (bp != NULL) {
    if (queue->last) {
      queue->last->m_nextpkt = bp;
      queue->last = bp;
    } else
      queue->last = queue->top = bp;
    queue->len++;
    log_Printf(LogDEBUG, "m_enqueue: len = %d\n", queue->len);
  }
}

struct mbuf *
m_pullup(struct mbuf *bp)
{
  /* Put it all in one contigous (aligned) mbuf */

  if (bp != NULL) {
    if (bp->m_next != NULL) {
      struct mbuf *nbp;
      u_char *cp;

      nbp = m_get(m_length(bp), bp->m_type);

      for (cp = MBUF_CTOP(nbp); bp; bp = m_free(bp)) {
        memcpy(cp, MBUF_CTOP(bp), bp->m_len);
        cp += bp->m_len;
      }
      bp = nbp;
    }
#ifndef __i386__	/* Do any other archs not care about alignment ? */
    else if ((bp->m_offset & (sizeof(long) - 1)) != 0) {
      bcopy(MBUF_CTOP(bp), bp + 1, bp->m_len);
      bp->m_offset = 0;
    }
#endif
  }

  return bp;
}

void
m_settype(struct mbuf *bp, int type)
{
  for (; bp; bp = bp->m_next)
    if (type != bp->m_type) {
      MemMap[bp->m_type].fragments--;
      MemMap[bp->m_type].octets -= bp->m_size;
      bp->m_type = type;
      MemMap[type].fragments++;
      MemMap[type].octets += bp->m_size;
    }
}

struct mbuf *
m_append(struct mbuf *m, const void *v, size_t sz)
{
  if (m) {
    while (m->m_next)
      m = m->m_next;
    if (m->m_size - m->m_len > sz)
      m->m_len += sz;
    else
      m->m_next = m_prepend(NULL, v, sz, 0);
  } else
    m = m_prepend(NULL, v, sz, 0);

  return m;
}
