/*
 * Written by Eivind Eklund <eivind@yes.no>
 *    for Yes Interactive
 *
 * Copyright (C) 1998, Yes Interactive.  All rights reserved.
 *
 * Redistribution and use in any form is permitted.  Redistribution in
 * source form should include the above copyright and this set of
 * conditions, because large sections american law seems to have been
 * created by a bunch of jerks on drugs that are now illegal, forcing
 * me to include this copyright-stuff instead of placing this in the
 * public domain.  The name of of 'Yes Interactive' or 'Eivind Eklund'
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  $Id: physical.c,v 1.14 1999/06/11 13:29:29 brian Exp $
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/tty.h>	/* TIOCOUTQ */
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/ioctl.h>
#include <util.h>
#else
#include <libutil.h>
#endif

#include "layer.h"
#ifndef NOALIAS
#include "alias_cmd.h"
#endif
#include "proto.h"
#include "acf.h"
#include "vjcomp.h"
#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "throughput.h"
#include "sync.h"
#include "async.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "prompt.h"
#include "chat.h"
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "tcp.h"
#include "udp.h"
#include "exec.h"
#include "tty.h"


static int physical_DescriptorWrite(struct descriptor *, struct bundle *,
                                    const fd_set *);
static void physical_DescriptorRead(struct descriptor *, struct bundle *,
                                    const fd_set *);

static int
physical_DeviceSize(void)
{
  return sizeof(struct device);
}

struct {
  struct device *(*create)(struct physical *);
  struct device *(*iov2device)(int, struct physical *, struct iovec *iov,
                               int *niov, int maxiov);
  int (*DeviceSize)(void);
} devices[] = {
  { tty_Create, tty_iov2device, tty_DeviceSize },
  { tcp_Create, tcp_iov2device, tcp_DeviceSize },
  { udp_Create, udp_iov2device, udp_DeviceSize },
  { exec_Create, exec_iov2device, exec_DeviceSize }
};

#define NDEVICES (sizeof devices / sizeof devices[0])

static int
physical_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e,
                   int *n)
{
  return physical_doUpdateSet(d, r, w, e, n, 0);
}

struct physical *
physical_Create(struct datalink *dl, int type)
{
  struct physical *p;

  p = (struct physical *)malloc(sizeof(struct physical));
  if (!p)
    return NULL;

  p->link.type = PHYSICAL_LINK;
  p->link.name = dl->name;
  p->link.len = sizeof *p;
  throughput_init(&p->link.throughput);

  memset(p->link.Queue, '\0', sizeof p->link.Queue);
  memset(p->link.proto_in, '\0', sizeof p->link.proto_in);
  memset(p->link.proto_out, '\0', sizeof p->link.proto_out);
  link_EmptyStack(&p->link);

  p->handler = NULL;
  p->desc.type = PHYSICAL_DESCRIPTOR;
  p->desc.UpdateSet = physical_UpdateSet;
  p->desc.IsSet = physical_IsSet;
  p->desc.Read = physical_DescriptorRead;
  p->desc.Write = physical_DescriptorWrite;
  p->type = type;

  hdlc_Init(&p->hdlc, &p->link.lcp);
  async_Init(&p->async);

  p->fd = -1;
  p->out = NULL;
  p->connect_count = 0;
  p->dl = dl;
  p->input.sz = 0;
  *p->name.full = '\0';
  p->name.base = p->name.full;

  p->Utmp = 0;
  p->session_owner = (pid_t)-1;

  p->cfg.rts_cts = MODEM_CTSRTS;
  p->cfg.speed = MODEM_SPEED;
  p->cfg.parity = CS8;
  memcpy(p->cfg.devlist, MODEM_LIST, sizeof MODEM_LIST);
  p->cfg.ndev = NMODEMS;
  p->cfg.cd.required = 0;
  p->cfg.cd.delay = DEF_CDDELAY;

  lcp_Init(&p->link.lcp, dl->bundle, &p->link, &dl->fsmp);
  ccp_Init(&p->link.ccp, dl->bundle, &p->link, &dl->fsmp);

  return p;
}

static const struct parity {
  const char *name;
  const char *name1;
  int set;
} validparity[] = {
  { "even", "P_EVEN", CS7 | PARENB },
  { "odd", "P_ODD", CS7 | PARENB | PARODD },
  { "none", "P_ZERO", CS8 },
  { NULL, 0 },
};

static int
GetParityValue(const char *str)
{
  const struct parity *pp;

  for (pp = validparity; pp->name; pp++) {
    if (strcasecmp(pp->name, str) == 0 ||
	strcasecmp(pp->name1, str) == 0) {
      return pp->set;
    }
  }
  return (-1);
}

int
physical_SetParity(struct physical *p, const char *str)
{
  struct termios rstio;
  int val;

  val = GetParityValue(str);
  if (val > 0) {
    p->cfg.parity = val;
    if (p->fd >= 0) {
      tcgetattr(p->fd, &rstio);
      rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
      rstio.c_cflag |= val;
      tcsetattr(p->fd, TCSADRAIN, &rstio);
    }
    return 0;
  }
  log_Printf(LogWARN, "%s: %s: Invalid parity\n", p->link.name, str);
  return -1;
}

int
physical_GetSpeed(struct physical *p)
{
  if (p->handler && p->handler->speed)
    return (*p->handler->speed)(p);

  return 115200;
}

int
physical_SetSpeed(struct physical *p, int speed)
{
  if (IntToSpeed(speed) != B0) {
      p->cfg.speed = speed;
      return 1;
  }

  return 0;
}

int
physical_Raw(struct physical *p)
{
  if (p->handler && p->handler->raw)
    return (*p->handler->raw)(p);

  return 1;
}

void
physical_Offline(struct physical *p)
{
  if (p->handler && p->handler->offline)
    (*p->handler->offline)(p);
  log_Printf(LogPHASE, "%s: Disconnected!\n", p->link.name);
}

static int
physical_Lock(struct physical *p)
{
  int res;

  if (*p->name.full == '/' && p->type != PHYS_DIRECT &&
      (res = ID0uu_lock(p->name.base)) != UU_LOCK_OK) {
    if (res == UU_LOCK_INUSE)
      log_Printf(LogPHASE, "%s: %s is in use\n", p->link.name, p->name.full);
    else
      log_Printf(LogPHASE, "%s: %s is in use: uu_lock: %s\n",
                 p->link.name, p->name.full, uu_lockerr(res));
    return 0;
  }

  return 1;
}

static void
physical_Unlock(struct physical *p)
{
  char fn[MAXPATHLEN];
  if (*p->name.full == '/' && p->type != PHYS_DIRECT &&
      ID0uu_unlock(p->name.base) == -1)
    log_Printf(LogALERT, "%s: Can't uu_unlock %s\n", p->link.name, fn);
}

void
physical_Close(struct physical *p)
{
  int newsid;
  char fn[MAXPATHLEN];

  if (p->fd < 0)
    return;

  log_Printf(LogDEBUG, "%s: Close\n", p->link.name);

  if (p->handler && p->handler->cooked)
    (*p->handler->cooked)(p);

  physical_StopDeviceTimer(p);
  if (p->Utmp) {
    ID0logout(p->name.base);
    p->Utmp = 0;
  }
  newsid = tcgetpgrp(p->fd) == getpgrp();
  close(p->fd);
  p->fd = -1;
  log_SetTtyCommandMode(p->dl);

  throughput_stop(&p->link.throughput);
  throughput_log(&p->link.throughput, LogPHASE, p->link.name);

  if (p->session_owner != (pid_t)-1) {
    ID0kill(p->session_owner, SIGHUP);
    p->session_owner = (pid_t)-1;
  }

  if (newsid)
    bundle_setsid(p->dl->bundle, 0);

  if (*p->name.full == '/') {
    snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, p->name.base);
#ifndef RELEASE_CRUNCH
    if (ID0unlink(fn) == -1)
      log_Printf(LogALERT, "%s: Can't remove %s: %s\n",
                 p->link.name, fn, strerror(errno));
#else
    ID0unlink(fn);
#endif
  }
  physical_Unlock(p);
  if (p->handler && p->handler->destroy)
    (*p->handler->destroy)(p);
  p->handler = NULL;
  p->name.base = p->name.full;
  *p->name.full = '\0';
}

void
physical_Destroy(struct physical *p)
{
  physical_Close(p);
  free(p);
}

static int
physical_DescriptorWrite(struct descriptor *d, struct bundle *bundle,
                         const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  int nw, result = 0;

  if (p->out == NULL)
    p->out = link_Dequeue(&p->link);

  if (p->out) {
    nw = physical_Write(p, MBUF_CTOP(p->out), p->out->cnt);
    log_Printf(LogDEBUG, "%s: DescriptorWrite: wrote %d(%d) to %d\n",
               p->link.name, nw, p->out->cnt, p->fd);
    if (nw > 0) {
      p->out->cnt -= nw;
      p->out->offset += nw;
      if (p->out->cnt == 0)
	p->out = mbuf_FreeSeg(p->out);
      result = 1;
    } else if (nw < 0) {
      if (errno != EAGAIN) {
	log_Printf(LogPHASE, "%s: write (%d): %s\n", p->link.name,
                   p->fd, strerror(errno));
        datalink_Down(p->dl, CLOSE_NORMAL);
      }
      result = 1;
    }
    /* else we shouldn't really have been called !  select() is broken ! */
  }

  return result;
}

int
physical_ShowStatus(struct cmdargs const *arg)
{
  struct physical *p = arg->cx->physical;
  const char *dev;
  int n;

  prompt_Printf(arg->prompt, "Name: %s\n", p->link.name);
  prompt_Printf(arg->prompt, " State:           ");
  if (p->fd < 0)
    prompt_Printf(arg->prompt, "closed\n");
  else if (p->handler && p->handler->openinfo)
    prompt_Printf(arg->prompt, "open (%s)\n", (*p->handler->openinfo)(p));
  else
    prompt_Printf(arg->prompt, "open\n");

  prompt_Printf(arg->prompt, " Device:          %s",
                *p->name.full ?  p->name.full :
                p->type == PHYS_DIRECT ? "unknown" : "N/A");
  if (p->session_owner != (pid_t)-1)
    prompt_Printf(arg->prompt, " (session owner: %d)", (int)p->session_owner);

  prompt_Printf(arg->prompt, "\n Link Type:       %s\n", mode2Nam(p->type));
  prompt_Printf(arg->prompt, " Connect Count:   %d\n", p->connect_count);
#ifdef TIOCOUTQ
  if (p->fd >= 0 && ioctl(p->fd, TIOCOUTQ, &n) >= 0)
      prompt_Printf(arg->prompt, " Physical outq:   %d\n", n);
#endif

  prompt_Printf(arg->prompt, " Queued Packets:  %d\n",
                link_QueueLen(&p->link));
  prompt_Printf(arg->prompt, " Phone Number:    %s\n", arg->cx->phone.chosen);

  prompt_Printf(arg->prompt, "\nDefaults:\n");

  prompt_Printf(arg->prompt, " Device List:     ");
  dev = p->cfg.devlist;
  for (n = 0; n < p->cfg.ndev; n++) {
    if (n)
      prompt_Printf(arg->prompt, ", ");
    prompt_Printf(arg->prompt, "\"%s\"", dev);
    dev += strlen(dev) + 1;
  }
  
  prompt_Printf(arg->prompt, "\n Characteristics: ");
  if (physical_IsSync(arg->cx->physical))
    prompt_Printf(arg->prompt, "sync");
  else
    prompt_Printf(arg->prompt, "%dbps", p->cfg.speed);

  switch (p->cfg.parity & CSIZE) {
  case CS7:
    prompt_Printf(arg->prompt, ", cs7");
    break;
  case CS8:
    prompt_Printf(arg->prompt, ", cs8");
    break;
  }
  if (p->cfg.parity & PARENB) {
    if (p->cfg.parity & PARODD)
      prompt_Printf(arg->prompt, ", odd parity");
    else
      prompt_Printf(arg->prompt, ", even parity");
  } else
    prompt_Printf(arg->prompt, ", no parity");

  prompt_Printf(arg->prompt, ", CTS/RTS %s\n", (p->cfg.rts_cts ? "on" : "off"));

  prompt_Printf(arg->prompt, " CD check delay:  %d second%s",
                p->cfg.cd.delay, p->cfg.cd.delay == 1 ? "" : "s");
  if (p->cfg.cd.required)
    prompt_Printf(arg->prompt, " (required!)\n\n");
  else
    prompt_Printf(arg->prompt, "\n\n");

  throughput_disp(&p->link.throughput, arg->prompt);

  return 0;
}

static void
physical_DescriptorRead(struct descriptor *d, struct bundle *bundle,
                     const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  u_char *rbuff;
  int n, found;

  rbuff = p->input.buf + p->input.sz;

  /* something to read */
  n = physical_Read(p, rbuff, sizeof p->input.buf - p->input.sz);
  log_Printf(LogDEBUG, "%s: DescriptorRead: read %d/%d from %d\n",
             p->link.name, n, (int)(sizeof p->input.buf - p->input.sz), p->fd);
  if (n <= 0) {
    if (n < 0)
      log_Printf(LogPHASE, "%s: read (%d): %s\n", p->link.name, p->fd,
                 strerror(errno));
    else
      log_Printf(LogPHASE, "%s: read (%d): Got zero bytes\n",
                 p->link.name, p->fd);
    datalink_Down(p->dl, CLOSE_NORMAL);
    return;
  }

  rbuff -= p->input.sz;
  n += p->input.sz;

  if (p->link.lcp.fsm.state <= ST_CLOSED) {
    if (p->type != PHYS_DEDICATED) {
      found = hdlc_Detect((u_char const **)&rbuff, n, physical_IsSync(p));
      if (rbuff != p->input.buf)
        log_WritePrompts(p->dl, "%.*s", (int)(rbuff - p->input.buf),
                         p->input.buf);
      p->input.sz = n - (rbuff - p->input.buf);

      if (found) {
        /* LCP packet is detected. Turn ourselves into packet mode */
        log_Printf(LogPHASE, "%s: PPP packet detected, coming up\n",
                   p->link.name);
        log_SetTtyCommandMode(p->dl);
        datalink_Up(p->dl, 0, 1);
        link_PullPacket(&p->link, rbuff, p->input.sz, bundle);
        p->input.sz = 0;
      } else
        bcopy(rbuff, p->input.buf, p->input.sz);
    } else
      /* In -dedicated mode, we just discard input until LCP is started */
      p->input.sz = 0;
  } else if (n > 0)
    link_PullPacket(&p->link, rbuff, n, bundle);
}

struct physical *
iov2physical(struct datalink *dl, struct iovec *iov, int *niov, int maxiov,
             int fd)
{
  struct physical *p;
  int len, h, type;

  p = (struct physical *)iov[(*niov)++].iov_base;
  p->link.name = dl->name;
  throughput_init(&p->link.throughput);
  memset(p->link.Queue, '\0', sizeof p->link.Queue);

  p->desc.UpdateSet = physical_UpdateSet;
  p->desc.IsSet = physical_IsSet;
  p->desc.Read = physical_DescriptorRead;
  p->desc.Write = physical_DescriptorWrite;
  p->type = PHYS_DIRECT;
  p->dl = dl;
  len = strlen(_PATH_DEV);
  p->out = NULL;
  p->connect_count = 1;

  physical_SetDevice(p, p->name.full);

  p->link.lcp.fsm.bundle = dl->bundle;
  p->link.lcp.fsm.link = &p->link;
  memset(&p->link.lcp.fsm.FsmTimer, '\0', sizeof p->link.lcp.fsm.FsmTimer);
  memset(&p->link.lcp.fsm.OpenTimer, '\0', sizeof p->link.lcp.fsm.OpenTimer);
  memset(&p->link.lcp.fsm.StoppedTimer, '\0',
         sizeof p->link.lcp.fsm.StoppedTimer);
  p->link.lcp.fsm.parent = &dl->fsmp;
  lcp_SetupCallbacks(&p->link.lcp);

  p->link.ccp.fsm.bundle = dl->bundle;
  p->link.ccp.fsm.link = &p->link;
  /* Our in.state & out.state are NULL (no link-level ccp yet) */
  memset(&p->link.ccp.fsm.FsmTimer, '\0', sizeof p->link.ccp.fsm.FsmTimer);
  memset(&p->link.ccp.fsm.OpenTimer, '\0', sizeof p->link.ccp.fsm.OpenTimer);
  memset(&p->link.ccp.fsm.StoppedTimer, '\0',
         sizeof p->link.ccp.fsm.StoppedTimer);
  p->link.ccp.fsm.parent = &dl->fsmp;
  ccp_SetupCallbacks(&p->link.ccp);

  p->hdlc.lqm.owner = &p->link.lcp;
  p->hdlc.ReportTimer.state = TIMER_STOPPED;
  p->hdlc.lqm.timer.state = TIMER_STOPPED;

  p->fd = fd;

  type = (long)p->handler;
  p->handler = NULL;
  for (h = 0; h < NDEVICES && p->handler == NULL; h++)
    p->handler = (*devices[h].iov2device)(type, p, iov, niov, maxiov);

  if (p->handler == NULL) {
    log_Printf(LogPHASE, "%s: Device %s, unknown link type\n",
               p->link.name, p->name.full);
    free(iov[(*niov)++].iov_base);
    physical_SetupStack(p, "unknown", PHYSICAL_NOFORCE);
  } else
    log_Printf(LogPHASE, "%s: Device %s, link type is %s\n",
               p->link.name, p->name.full, p->handler->name);

  if (p->hdlc.lqm.method && p->hdlc.lqm.timer.load)
    lqr_reStart(&p->link.lcp);
  hdlc_StartTimer(&p->hdlc);

  throughput_start(&p->link.throughput, "physical throughput",
                   Enabled(dl->bundle, OPT_THROUGHPUT));

  return p;
}

int
physical_MaxDeviceSize()
{
  int biggest, sz, n;

  biggest = sizeof(struct device);
  for (sz = n = 0; n < NDEVICES; n++)
    if (devices[n].DeviceSize) {
      sz = (*devices[n].DeviceSize)();
      if (biggest < sz)
        biggest = sz;
    }

  return biggest;
}

int
physical2iov(struct physical *p, struct iovec *iov, int *niov, int maxiov,
             pid_t newpid)
{
  struct device *h;
  int sz;

  h = NULL;
  if (p) {
    hdlc_StopTimer(&p->hdlc);
    lqr_StopTimer(p);
    timer_Stop(&p->link.lcp.fsm.FsmTimer);
    timer_Stop(&p->link.ccp.fsm.FsmTimer);
    timer_Stop(&p->link.lcp.fsm.OpenTimer);
    timer_Stop(&p->link.ccp.fsm.OpenTimer);
    timer_Stop(&p->link.lcp.fsm.StoppedTimer);
    timer_Stop(&p->link.ccp.fsm.StoppedTimer);
    if (p->handler) {
      if (p->handler->device2iov)
        h = p->handler;
      p->handler = (struct device *)(long)p->handler->type;
    }

    if (Enabled(p->dl->bundle, OPT_KEEPSESSION) ||
        tcgetpgrp(p->fd) == getpgrp())
      p->session_owner = getpid();      /* So I'll eventually get HUP'd */
    else
      p->session_owner = (pid_t)-1;
    timer_Stop(&p->link.throughput.Timer);
    physical_ChangedPid(p, newpid);
  }

  if (*niov + 1 >= maxiov) {
    log_Printf(LogERROR, "physical2iov: No room for physical + device !\n");
    if (p)
      free(p);
    return -1;
  }

  iov[*niov].iov_base = p ? (void *)p : malloc(sizeof *p);
  iov[*niov].iov_len = sizeof *p;
  (*niov)++;

  sz = physical_MaxDeviceSize();
  if (p) {
    if (h)
      (*h->device2iov)(h, iov, niov, maxiov, newpid);
    else {
      iov[*niov].iov_base = malloc(sz);
      if (p->handler)
        memcpy(iov[*niov].iov_base, p->handler, sizeof *p->handler);
      iov[*niov].iov_len = sz;
      (*niov)++;
    }
  } else {
    iov[*niov].iov_base = malloc(sz);
    iov[*niov].iov_len = sz;
    (*niov)++;
  }

  return p ? p->fd : 0;
}

void
physical_ChangedPid(struct physical *p, pid_t newpid)
{
  if (p->fd >= 0 && *p->name.full == '/' && p->type != PHYS_DIRECT) {
    int res;

    if ((res = ID0uu_lock_txfr(p->name.base, newpid)) != UU_LOCK_OK)
      log_Printf(LogPHASE, "uu_lock_txfr: %s\n", uu_lockerr(res));
  }
}

int
physical_IsSync(struct physical *p)
{
   return p->cfg.speed == 0;
}

const char *physical_GetDevice(struct physical *p)
{
   return p->name.full;
}

void
physical_SetDeviceList(struct physical *p, int argc, const char *const *argv)
{
  int f, pos;

  p->cfg.devlist[sizeof p->cfg.devlist - 1] = '\0';
  for (f = 0, pos = 0; f < argc && pos < sizeof p->cfg.devlist - 1; f++) {
    if (pos)
      p->cfg.devlist[pos++] = '\0';
    strncpy(p->cfg.devlist + pos, argv[f], sizeof p->cfg.devlist - pos - 1);
    pos += strlen(p->cfg.devlist + pos);
  }
  p->cfg.ndev = f;
}

void
physical_SetSync(struct physical *p)
{
   p->cfg.speed = 0;
}

int
physical_SetRtsCts(struct physical *p, int enable)
{
   p->cfg.rts_cts = enable ? 1 : 0;
   return 1;
}

ssize_t
physical_Read(struct physical *p, void *buf, size_t nbytes)
{
  ssize_t ret;

  if (p->handler && p->handler->read)
    ret = (*p->handler->read)(p, buf, nbytes);
  else
    ret = read(p->fd, buf, nbytes);

  log_DumpBuff(LogPHYSICAL, "read", buf, ret);

  return ret;
}

ssize_t
physical_Write(struct physical *p, const void *buf, size_t nbytes)
{
  log_DumpBuff(LogPHYSICAL, "write", buf, nbytes);

  if (p->handler && p->handler->write)
    return (*p->handler->write)(p, buf, nbytes);

  return write(p->fd, buf, nbytes);
}

int
physical_doUpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e,
                     int *n, int force)
{
  struct physical *p = descriptor2physical(d);
  int sets;

  sets = 0;
  if (p->fd >= 0) {
    if (r) {
      FD_SET(p->fd, r);
      log_Printf(LogTIMER, "%s: fdset(r) %d\n", p->link.name, p->fd);
      sets++;
    }
    if (e) {
      FD_SET(p->fd, e);
      log_Printf(LogTIMER, "%s: fdset(e) %d\n", p->link.name, p->fd);
      sets++;
    }
    if (w && (force || link_QueueLen(&p->link) || p->out)) {
      FD_SET(p->fd, w);
      log_Printf(LogTIMER, "%s: fdset(w) %d\n", p->link.name, p->fd);
      sets++;
    }
    if (sets && *n < p->fd + 1)
      *n = p->fd + 1;
  }

  return sets;
}

int
physical_RemoveFromSet(struct physical *p, fd_set *r, fd_set *w, fd_set *e)
{
  int sets;

  sets = 0;
  if (p->fd >= 0) {
    if (r && FD_ISSET(p->fd, r)) {
      FD_CLR(p->fd, r);
      log_Printf(LogTIMER, "%s: fdunset(r) %d\n", p->link.name, p->fd);
      sets++;
    }
    if (e && FD_ISSET(p->fd, e)) {
      FD_CLR(p->fd, e);
      log_Printf(LogTIMER, "%s: fdunset(e) %d\n", p->link.name, p->fd);
      sets++;
    }
    if (w && FD_ISSET(p->fd, w)) {
      FD_CLR(p->fd, w);
      log_Printf(LogTIMER, "%s: fdunset(w) %d\n", p->link.name, p->fd);
      sets++;
    }
  }
  
  return sets;
}

int
physical_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  return p->fd >= 0 && FD_ISSET(p->fd, fdset);
}

void
physical_Login(struct physical *p, const char *name)
{
  if (p->type == PHYS_DIRECT && *p->name.base && !p->Utmp) {
    struct utmp ut;
    const char *connstr;

    memset(&ut, 0, sizeof ut);
    time(&ut.ut_time);
    strncpy(ut.ut_name, name, sizeof ut.ut_name);
    strncpy(ut.ut_line, p->name.base, sizeof ut.ut_line);
    if ((connstr = getenv("CONNECT")))
      /* mgetty sets this to the connection speed */
      strncpy(ut.ut_host, connstr, sizeof ut.ut_host);
    ID0login(&ut);
    p->Utmp = 1;
  }
}

int
physical_SetMode(struct physical *p, int mode)
{
  if ((p->type & (PHYS_DIRECT|PHYS_DEDICATED) ||
       mode & (PHYS_DIRECT|PHYS_DEDICATED)) &&
      (!(p->type & PHYS_DIRECT) || !(mode & PHYS_BACKGROUND))) {
    log_Printf(LogWARN, "%s: Cannot change mode %s to %s\n", p->link.name,
               mode2Nam(p->type), mode2Nam(mode));
    return 0;
  }
  p->type = mode;
  return 1;
}

void
physical_DeleteQueue(struct physical *p)
{
  if (p->out) {
    mbuf_Free(p->out);
    p->out = NULL;
  }
  link_DeleteQueue(&p->link);
}

void
physical_SetDevice(struct physical *p, const char *name)
{
  int len = strlen(_PATH_DEV);

  if (name != p->name.full) {
    strncpy(p->name.full, name, sizeof p->name.full - 1);
    p->name.full[sizeof p->name.full - 1] = '\0';
  }
  p->name.base = *p->name.full == '!' ?  p->name.full + 1 :
                 strncmp(p->name.full, _PATH_DEV, len) ?
                 p->name.full : p->name.full + len;
}

static void
physical_Found(struct physical *p)
{
  FILE *lockfile;
  char fn[MAXPATHLEN];

  if (*p->name.full == '/') {
    snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, p->name.base);
    lockfile = ID0fopen(fn, "w");
    if (lockfile != NULL) {
      fprintf(lockfile, "%s%d\n", TUN_NAME, p->dl->bundle->unit);
      fclose(lockfile);
    }
#ifndef RELEASE_CRUNCH
    else
      log_Printf(LogALERT, "%s: Can't create %s: %s\n",
                 p->link.name, fn, strerror(errno));
#endif
  }

  throughput_start(&p->link.throughput, "physical throughput",
                   Enabled(p->dl->bundle, OPT_THROUGHPUT));
  p->connect_count++;
  p->input.sz = 0;

  log_Printf(LogPHASE, "%s: Connected!\n", p->link.name);
}

int
physical_Open(struct physical *p, struct bundle *bundle)
{
  int devno, h, wasopen, err;
  char *dev;

  if (p->fd >= 0)
    log_Printf(LogDEBUG, "%s: Open: Modem is already open!\n", p->link.name);
    /* We're going back into "term" mode */
  else if (p->type == PHYS_DIRECT) {
    physical_SetDevice(p, "");
    p->fd = STDIN_FILENO;
    for (h = 0; h < NDEVICES && p->handler == NULL && p->fd >= 0; h++)
        p->handler = (*devices[h].create)(p);
    if (p->fd >= 0) {
      if (p->handler == NULL) {
        physical_SetupStack(p, "unknown", PHYSICAL_NOFORCE);
        log_Printf(LogDEBUG, "%s: stdin is unidentified\n", p->link.name);
      }
      physical_Found(p);
    }
  } else {
    dev = p->cfg.devlist;
    devno = 0;
    while (devno < p->cfg.ndev && p->fd < 0) {
      physical_SetDevice(p, dev);
      if (physical_Lock(p)) {
        err = 0;

        if (*p->name.full == '/') {
          p->fd = ID0open(p->name.full, O_RDWR | O_NONBLOCK);
          if (p->fd < 0)
            err = errno;
        }

        wasopen = p->fd >= 0;
        for (h = 0; h < NDEVICES && p->handler == NULL; h++)
          if ((p->handler = (*devices[h].create)(p)) == NULL &&
              wasopen && p->fd == -1)
            break;

        if (p->fd < 0) {
          if (h == NDEVICES) {
            if (err)
	      log_Printf(LogWARN, "%s: %s: %s\n", p->link.name, p->name.full,
                         strerror(errno));
            else
	      log_Printf(LogWARN, "%s: Device (%s) must begin with a '/',"
                         " a '!' or be a host:port pair\n", p->link.name,
                         p->name.full);
          }
          physical_Unlock(p);
        } else
          physical_Found(p);
      }
      dev += strlen(dev) + 1;
      devno++;
    }
  }

  return p->fd;
}

void
physical_SetupStack(struct physical *p, const char *who, int how)
{
  link_EmptyStack(&p->link);
  if (how == PHYSICAL_FORCE_SYNC ||
      (how == PHYSICAL_NOFORCE && physical_IsSync(p)))
    link_Stack(&p->link, &synclayer);
  else {
    link_Stack(&p->link, &asynclayer);
    link_Stack(&p->link, &hdlclayer);
  }
  link_Stack(&p->link, &acflayer);
  link_Stack(&p->link, &protolayer);
  link_Stack(&p->link, &lqrlayer);
  link_Stack(&p->link, &ccplayer);
  link_Stack(&p->link, &vjlayer);
#ifndef NOALIAS
  link_Stack(&p->link, &aliaslayer);
#endif
  if (how == PHYSICAL_FORCE_ASYNC && physical_IsSync(p)) {
    log_Printf(LogWARN, "Sync device setting ignored for ``%s'' device\n", who);
    p->cfg.speed = MODEM_SPEED;
  } else if (how == PHYSICAL_FORCE_SYNC && !physical_IsSync(p)) {
    log_Printf(LogWARN, "Async device setting ignored for ``%s'' device\n",
               who);
    physical_SetSync(p);
  }
}

void
physical_StopDeviceTimer(struct physical *p)
{
  if (p->handler && p->handler->stoptimer)
    (*p->handler->stoptimer)(p);
}
