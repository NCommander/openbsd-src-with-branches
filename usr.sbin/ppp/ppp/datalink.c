/*-
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: datalink.c,v 1.28 1999/02/02 09:35:17 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <termios.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "descriptor.h"
#include "lqr.h"
#include "hdlc.h"
#include "async.h"
#include "throughput.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "chat.h"
#include "auth.h"
#include "modem.h"
#include "prompt.h"
#include "lcpproto.h"
#include "pap.h"
#include "chap.h"
#include "command.h"
#include "cbcp.h"
#include "datalink.h"

static void datalink_LoginDone(struct datalink *);
static void datalink_NewState(struct datalink *, int);

static void
datalink_OpenTimeout(void *v)
{
  struct datalink *dl = (struct datalink *)v;

  timer_Stop(&dl->dial_timer);
  if (dl->state == DATALINK_OPENING)
    log_Printf(LogPHASE, "%s: Redial timer expired.\n", dl->name);
}

static void
datalink_StartDialTimer(struct datalink *dl, int Timeout)
{
  timer_Stop(&dl->dial_timer);
 
  if (Timeout) {
    if (Timeout > 0)
      dl->dial_timer.load = Timeout * SECTICKS;
    else
      dl->dial_timer.load = (random() % DIAL_TIMEOUT) * SECTICKS;
    dl->dial_timer.func = datalink_OpenTimeout;
    dl->dial_timer.name = "dial";
    dl->dial_timer.arg = dl;
    timer_Start(&dl->dial_timer);
    if (dl->state == DATALINK_OPENING)
      log_Printf(LogPHASE, "%s: Enter pause (%d) for redialing.\n",
                dl->name, Timeout);
  }
}

static void
datalink_HangupDone(struct datalink *dl)
{
  if (dl->physical->type == PHYS_DEDICATED && !dl->bundle->CleaningUp &&
      physical_GetFD(dl->physical) != -1) {
    /* Don't close our modem if the link is dedicated */
    datalink_LoginDone(dl);
    return;
  }

  modem_Close(dl->physical);
  dl->phone.chosen = "N/A";

  if (dl->cbcp.required) {
    log_Printf(LogPHASE, "Call peer back on %s\n", dl->cbcp.fsm.phone);
    dl->cfg.callback.opmask = 0;
    strncpy(dl->cfg.phone.list, dl->cbcp.fsm.phone,
            sizeof dl->cfg.phone.list - 1);
    dl->cfg.phone.list[sizeof dl->cfg.phone.list - 1] = '\0';
    dl->phone.alt = dl->phone.next = NULL;
    dl->reconnect_tries = dl->cfg.reconnect.max;
    dl->dial_tries = dl->cfg.dial.max;
    dl->script.run = 1;
    dl->script.packetmode = 1;
    if (!physical_SetMode(dl->physical, PHYS_BACKGROUND))
      log_Printf(LogERROR, "Oops - can't change mode to BACKGROUND (gulp) !\n");
    bundle_LinksRemoved(dl->bundle);
    if (dl->cbcp.fsm.delay < dl->cfg.dial.timeout)
      dl->cbcp.fsm.delay = dl->cfg.dial.timeout;
    datalink_StartDialTimer(dl, dl->cbcp.fsm.delay);
    cbcp_Down(&dl->cbcp);
    datalink_NewState(dl, DATALINK_OPENING);
  } else if (dl->bundle->CleaningUp ||
      (dl->physical->type == PHYS_DIRECT) ||
      ((!dl->dial_tries || (dl->dial_tries < 0 && !dl->reconnect_tries)) &&
       !(dl->physical->type & (PHYS_DDIAL|PHYS_DEDICATED)))) {
    datalink_NewState(dl, DATALINK_CLOSED);
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
    bundle_LinkClosed(dl->bundle, dl);
    if (!dl->bundle->CleaningUp)
      datalink_StartDialTimer(dl, dl->cfg.dial.timeout);
  } else {
    datalink_NewState(dl, DATALINK_OPENING);
    if (dl->dial_tries < 0) {
      datalink_StartDialTimer(dl, dl->cfg.reconnect.timeout);
      dl->dial_tries = dl->cfg.dial.max;
      dl->reconnect_tries--;
    } else {
      if (dl->phone.next == NULL)
        datalink_StartDialTimer(dl, dl->cfg.dial.timeout);
      else
        datalink_StartDialTimer(dl, dl->cfg.dial.next_timeout);
    }
  }
}

static const char *
datalink_ChoosePhoneNumber(struct datalink *dl)
{
  char *phone;

  if (dl->phone.alt == NULL) {
    if (dl->phone.next == NULL) {
      strncpy(dl->phone.list, dl->cfg.phone.list, sizeof dl->phone.list - 1);
      dl->phone.list[sizeof dl->phone.list - 1] = '\0';
      dl->phone.next = dl->phone.list;
    }
    dl->phone.alt = strsep(&dl->phone.next, ":");
  }
  phone = strsep(&dl->phone.alt, "|");
  dl->phone.chosen = *phone ? phone : "[NONE]";
  if (*phone)
    log_Printf(LogPHASE, "Phone: %s\n", phone);
  return phone;
}

static void
datalink_LoginDone(struct datalink *dl)
{
  if (!dl->script.packetmode) { 
    dl->dial_tries = -1;
    datalink_NewState(dl, DATALINK_READY);
  } else if (modem_Raw(dl->physical, dl->bundle) < 0) {
    dl->dial_tries = 0;
    log_Printf(LogWARN, "datalink_LoginDone: Not connected.\n");
    if (dl->script.run) { 
      datalink_NewState(dl, DATALINK_HANGUP);
      modem_Offline(dl->physical);
      chat_Init(&dl->chat, dl->physical, dl->cfg.script.hangup, 1, NULL);
    } else {
      timer_Stop(&dl->physical->Timer);
      if (dl->physical->type == PHYS_DEDICATED)
        /* force a redial timeout */
        modem_Close(dl->physical);
      datalink_HangupDone(dl);
    }
  } else {
    dl->dial_tries = -1;

    hdlc_Init(&dl->physical->hdlc, &dl->physical->link.lcp);
    async_Init(&dl->physical->async);

    lcp_Setup(&dl->physical->link.lcp, dl->state == DATALINK_READY ?
              0 : dl->physical->link.lcp.cfg.openmode);
    ccp_Setup(&dl->physical->link.ccp);

    datalink_NewState(dl, DATALINK_LCP);
    fsm_Up(&dl->physical->link.lcp.fsm);
    fsm_Open(&dl->physical->link.lcp.fsm);
  }
}

static int
datalink_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e,
                   int *n)
{
  struct datalink *dl = descriptor2datalink(d);
  int result;

  result = 0;
  switch (dl->state) {
    case DATALINK_CLOSED:
      if ((dl->physical->type &
           (PHYS_DIRECT|PHYS_DEDICATED|PHYS_BACKGROUND|PHYS_DDIAL)) &&
          !bundle_IsDead(dl->bundle))
        /*
         * Our first time in - DEDICATED & DDIAL never come down, and
         * DIRECT & BACKGROUND get deleted when they enter DATALINK_CLOSED.
         * Go to DATALINK_OPENING via datalink_Up() and fall through.
         */
        datalink_Up(dl, 1, 1);
      else
        break;
      /* fall through */

    case DATALINK_OPENING:
      if (dl->dial_timer.state != TIMER_RUNNING) {
        if (--dl->dial_tries < 0)
          dl->dial_tries = 0;
        if (modem_Open(dl->physical, dl->bundle) >= 0) {
          log_WritePrompts(dl, "%s: Entering terminal mode on %s\r\n"
                           "Type `~?' for help\r\n", dl->name,
                           dl->physical->name.full);
          if (dl->script.run) {
            datalink_NewState(dl, DATALINK_DIAL);
            chat_Init(&dl->chat, dl->physical, dl->cfg.script.dial, 1,
                      datalink_ChoosePhoneNumber(dl));
            if (!(dl->physical->type & (PHYS_DDIAL|PHYS_DEDICATED)) &&
                dl->cfg.dial.max)
              log_Printf(LogCHAT, "%s: Dial attempt %u of %d\n",
                        dl->name, dl->cfg.dial.max - dl->dial_tries,
                        dl->cfg.dial.max);
          } else
            datalink_LoginDone(dl);
          return datalink_UpdateSet(d, r, w, e, n);
        } else {
          if (!(dl->physical->type & (PHYS_DDIAL|PHYS_DEDICATED)) &&
              dl->cfg.dial.max)
            log_Printf(LogCHAT, "Failed to open modem (attempt %u of %d)\n",
                       dl->cfg.dial.max - dl->dial_tries, dl->cfg.dial.max);
          else
            log_Printf(LogCHAT, "Failed to open modem\n");

          if (dl->bundle->CleaningUp ||
              (!(dl->physical->type & (PHYS_DDIAL|PHYS_DEDICATED)) &&
               dl->cfg.dial.max && dl->dial_tries == 0)) {
            datalink_NewState(dl, DATALINK_CLOSED);
            dl->reconnect_tries = 0;
            dl->dial_tries = -1;
            log_WritePrompts(dl, "Failed to open %s\n",
                             dl->physical->name.full);
            bundle_LinkClosed(dl->bundle, dl);
          }
          if (!dl->bundle->CleaningUp) {
            log_WritePrompts(dl, "Failed to open %s, pause %d seconds\n",
                             dl->physical->name.full, dl->cfg.dial.timeout);
            datalink_StartDialTimer(dl, dl->cfg.dial.timeout);
          }
        }
      }
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      result = descriptor_UpdateSet(&dl->chat.desc, r, w, e, n);
      switch (dl->chat.state) {
        case CHAT_DONE:
          /* script succeeded */
          chat_Destroy(&dl->chat);
          switch(dl->state) {
            case DATALINK_HANGUP:
              datalink_HangupDone(dl);
              break;
            case DATALINK_DIAL:
              datalink_NewState(dl, DATALINK_LOGIN);
              chat_Init(&dl->chat, dl->physical, dl->cfg.script.login, 0, NULL);
              return datalink_UpdateSet(d, r, w, e, n);
            case DATALINK_LOGIN:
              dl->phone.alt = NULL;
              datalink_LoginDone(dl);
              return datalink_UpdateSet(d, r, w, e, n);
          }
          break;
        case CHAT_FAILED:
          /* Going down - script failed */
          log_Printf(LogWARN, "Chat script failed\n");
          chat_Destroy(&dl->chat);
          switch(dl->state) {
            case DATALINK_HANGUP:
              datalink_HangupDone(dl);
              break;
            case DATALINK_DIAL:
            case DATALINK_LOGIN:
              datalink_NewState(dl, DATALINK_HANGUP);
              modem_Offline(dl->physical);	/* Is this required ? */
              chat_Init(&dl->chat, dl->physical, dl->cfg.script.hangup, 1, NULL);
              return datalink_UpdateSet(d, r, w, e, n);
          }
          break;
      }
      break;

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_CBCP:
    case DATALINK_OPEN:
      result = descriptor_UpdateSet(&dl->physical->desc, r, w, e, n);
      break;
  }
  return result;
}

int
datalink_RemoveFromSet(struct datalink *dl, fd_set *r, fd_set *w, fd_set *e)
{
  return physical_RemoveFromSet(dl->physical, r, w, e);
}

static int
datalink_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct datalink *dl = descriptor2datalink(d);

  switch (dl->state) {
    case DATALINK_CLOSED:
    case DATALINK_OPENING:
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      return descriptor_IsSet(&dl->chat.desc, fdset);

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_CBCP:
    case DATALINK_OPEN:
      return descriptor_IsSet(&dl->physical->desc, fdset);
  }
  return 0;
}

static void
datalink_Read(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct datalink *dl = descriptor2datalink(d);

  switch (dl->state) {
    case DATALINK_CLOSED:
    case DATALINK_OPENING:
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      descriptor_Read(&dl->chat.desc, bundle, fdset);
      break;

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_CBCP:
    case DATALINK_OPEN:
      descriptor_Read(&dl->physical->desc, bundle, fdset);
      break;
  }
}

static int
datalink_Write(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct datalink *dl = descriptor2datalink(d);
  int result = 0;

  switch (dl->state) {
    case DATALINK_CLOSED:
    case DATALINK_OPENING:
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      result = descriptor_Write(&dl->chat.desc, bundle, fdset);
      break;

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_CBCP:
    case DATALINK_OPEN:
      result = descriptor_Write(&dl->physical->desc, bundle, fdset);
      break;
  }

  return result;
}

static void
datalink_ComeDown(struct datalink *dl, int how)
{
  if (how != CLOSE_NORMAL) {
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
    if (dl->state >= DATALINK_READY && how == CLOSE_LCP)
      dl->stayonline = 1;
  }

  if (dl->state >= DATALINK_READY && dl->stayonline) {
    dl->stayonline = 0;
    timer_Stop(&dl->physical->Timer);
    datalink_NewState(dl, DATALINK_READY);
  } else if (dl->state != DATALINK_CLOSED && dl->state != DATALINK_HANGUP) {
    modem_Offline(dl->physical);
    chat_Destroy(&dl->chat);
    if (dl->script.run && dl->state != DATALINK_OPENING) {
      datalink_NewState(dl, DATALINK_HANGUP);
      chat_Init(&dl->chat, dl->physical, dl->cfg.script.hangup, 1, NULL);
    } else
      datalink_HangupDone(dl);
  }
}

static void
datalink_LayerStart(void *v, struct fsm *fp)
{
  /* The given FSM is about to start up ! */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP)
    (*dl->parent->LayerStart)(dl->parent->object, fp);
}

static void
datalink_LayerUp(void *v, struct fsm *fp)
{
  /* The given fsm is now up */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP) {
    datalink_GotAuthname(dl, "");
    dl->physical->link.lcp.auth_ineed = dl->physical->link.lcp.want_auth;
    dl->physical->link.lcp.auth_iwait = dl->physical->link.lcp.his_auth;
    if (dl->physical->link.lcp.his_auth || dl->physical->link.lcp.want_auth) {
      if (bundle_Phase(dl->bundle) == PHASE_ESTABLISH)
        bundle_NewPhase(dl->bundle, PHASE_AUTHENTICATE);
      log_Printf(LogPHASE, "%s: his = %s, mine = %s\n", dl->name,
                Auth2Nam(dl->physical->link.lcp.his_auth),
                Auth2Nam(dl->physical->link.lcp.want_auth));
      if (dl->physical->link.lcp.his_auth == PROTO_PAP)
        auth_StartReq(&dl->pap);
      if (dl->physical->link.lcp.want_auth == PROTO_CHAP)
        auth_StartReq(&dl->chap.auth);
    } else
      datalink_AuthOk(dl);
  }
}

void
datalink_GotAuthname(struct datalink *dl, const char *name)
{
  strncpy(dl->peer.authname, name, sizeof dl->peer.authname - 1);
  dl->peer.authname[sizeof dl->peer.authname - 1] = '\0';
}

void
datalink_NCPUp(struct datalink *dl)
{
  int ccpok = ccp_SetOpenMode(&dl->physical->link.ccp);

  if (dl->physical->link.lcp.want_mrru && dl->physical->link.lcp.his_mrru) {
    /* we've authenticated in multilink mode ! */
    switch (mp_Up(&dl->bundle->ncp.mp, dl)) {
      case MP_LINKSENT:
        /* We've handed the link off to another ppp (well, we will soon) ! */
        return;
      case MP_UP:
        /* First link in the bundle */
        auth_Select(dl->bundle, dl->peer.authname);
        /* fall through */
      case MP_ADDED:
        /* We're in multilink mode ! */
        dl->physical->link.ccp.fsm.open_mode = OPEN_PASSIVE;	/* override */
        break;
      case MP_FAILED:
        datalink_AuthNotOk(dl);
        return;
    }
  } else if (bundle_Phase(dl->bundle) == PHASE_NETWORK) {
    log_Printf(LogPHASE, "%s: Already in NETWORK phase\n", dl->name);
    datalink_NewState(dl, DATALINK_OPEN);
    (*dl->parent->LayerUp)(dl->parent->object, &dl->physical->link.lcp.fsm);
    return;
  } else {
    dl->bundle->ncp.mp.peer = dl->peer;
    ipcp_SetLink(&dl->bundle->ncp.ipcp, &dl->physical->link);
    auth_Select(dl->bundle, dl->peer.authname);
  }

  if (ccpok) {
    fsm_Up(&dl->physical->link.ccp.fsm);
    fsm_Open(&dl->physical->link.ccp.fsm);
  }
  datalink_NewState(dl, DATALINK_OPEN);
  bundle_NewPhase(dl->bundle, PHASE_NETWORK);
  (*dl->parent->LayerUp)(dl->parent->object, &dl->physical->link.lcp.fsm);
}

void
datalink_CBCPComplete(struct datalink *dl)
{
  datalink_NewState(dl, DATALINK_LCP);
  fsm_Close(&dl->physical->link.lcp.fsm);
}

void
datalink_CBCPFailed(struct datalink *dl)
{
  cbcp_Down(&dl->cbcp);
  datalink_CBCPComplete(dl);
}

void
datalink_AuthOk(struct datalink *dl)
{
  if ((dl->physical->link.lcp.his_callback.opmask &
       CALLBACK_BIT(CALLBACK_CBCP) ||
       dl->physical->link.lcp.want_callback.opmask &
       CALLBACK_BIT(CALLBACK_CBCP)) &&
      !(dl->physical->link.lcp.want_callback.opmask &
        CALLBACK_BIT(CALLBACK_AUTH))) {
    /* We must have agreed CBCP if AUTH isn't there any more */
    datalink_NewState(dl, DATALINK_CBCP);
    cbcp_Up(&dl->cbcp);
  } else if (dl->physical->link.lcp.want_callback.opmask) {
    /* It's not CBCP */
    log_Printf(LogPHASE, "%s: Shutdown and await peer callback\n", dl->name);
    datalink_NewState(dl, DATALINK_LCP);
    fsm_Close(&dl->physical->link.lcp.fsm);
  } else
    switch (dl->physical->link.lcp.his_callback.opmask) {
      case 0:
        datalink_NCPUp(dl);
        break;

      case CALLBACK_BIT(CALLBACK_AUTH):
        auth_SetPhoneList(dl->peer.authname, dl->cbcp.fsm.phone,
                          sizeof dl->cbcp.fsm.phone);
        if (*dl->cbcp.fsm.phone == '\0' || !strcmp(dl->cbcp.fsm.phone, "*")) {
          log_Printf(LogPHASE, "%s: %s cannot be called back\n", dl->name,
                     dl->peer.authname);
          *dl->cbcp.fsm.phone = '\0';
        } else {
          char *ptr = strchr(dl->cbcp.fsm.phone, ',');
          if (ptr)
            *ptr = '\0';	/* Call back on the first number */
          log_Printf(LogPHASE, "%s: Calling peer back on %s\n", dl->name,
                     dl->cbcp.fsm.phone);
          dl->cbcp.required = 1;
        }
        dl->cbcp.fsm.delay = 0;
        datalink_NewState(dl, DATALINK_LCP);
        fsm_Close(&dl->physical->link.lcp.fsm);
        break;

      case CALLBACK_BIT(CALLBACK_E164):
        strncpy(dl->cbcp.fsm.phone, dl->physical->link.lcp.his_callback.msg,
                sizeof dl->cbcp.fsm.phone - 1);
        dl->cbcp.fsm.phone[sizeof dl->cbcp.fsm.phone - 1] = '\0';
        log_Printf(LogPHASE, "%s: Calling peer back on %s\n", dl->name,
                   dl->cbcp.fsm.phone);
        dl->cbcp.required = 1;
        dl->cbcp.fsm.delay = 0;
        datalink_NewState(dl, DATALINK_LCP);
        fsm_Close(&dl->physical->link.lcp.fsm);
        break;

      default:
        log_Printf(LogPHASE, "%s: Oops - Should have NAK'd peer callback !\n",
                   dl->name);
        datalink_NewState(dl, DATALINK_LCP);
        fsm_Close(&dl->physical->link.lcp.fsm);
        break;
    }
}

void
datalink_AuthNotOk(struct datalink *dl)
{
  datalink_NewState(dl, DATALINK_LCP);
  fsm_Close(&dl->physical->link.lcp.fsm);
}

static void
datalink_LayerDown(void *v, struct fsm *fp)
{
  /* The given FSM has been told to come down */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP) {
    switch (dl->state) {
      case DATALINK_OPEN:
        peerid_Init(&dl->peer);
        fsm2initial(&dl->physical->link.ccp.fsm);
        datalink_NewState(dl, DATALINK_LCP);  /* before parent TLD */
        (*dl->parent->LayerDown)(dl->parent->object, fp);
        /* fall through (just in case) */

      case DATALINK_CBCP:
        if (!dl->cbcp.required)
          cbcp_Down(&dl->cbcp);
        /* fall through (just in case) */

      case DATALINK_AUTH:
        timer_Stop(&dl->pap.authtimer);
        timer_Stop(&dl->chap.auth.authtimer);
    }
    datalink_NewState(dl, DATALINK_LCP);
  }
}

static void
datalink_LayerFinish(void *v, struct fsm *fp)
{
  /* The given fsm is now down */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP) {
    fsm2initial(fp);
    (*dl->parent->LayerFinish)(dl->parent->object, fp);
    datalink_ComeDown(dl, CLOSE_NORMAL);
  } else if (fp->state == ST_CLOSED && fp->open_mode == OPEN_PASSIVE)
    fsm_Open(fp);		/* CCP goes to ST_STOPPED */
}

struct datalink *
datalink_Create(const char *name, struct bundle *bundle, int type)
{
  struct datalink *dl;

  dl = (struct datalink *)malloc(sizeof(struct datalink));
  if (dl == NULL)
    return dl;

  dl->desc.type = DATALINK_DESCRIPTOR;
  dl->desc.UpdateSet = datalink_UpdateSet;
  dl->desc.IsSet = datalink_IsSet;
  dl->desc.Read = datalink_Read;
  dl->desc.Write = datalink_Write;

  dl->state = DATALINK_CLOSED;

  *dl->cfg.script.dial = '\0';
  *dl->cfg.script.login = '\0';
  *dl->cfg.script.hangup = '\0';
  *dl->cfg.phone.list = '\0';
  *dl->phone.list = '\0';
  dl->phone.next = NULL;
  dl->phone.alt = NULL;
  dl->phone.chosen = "N/A";
  dl->stayonline = 0;
  dl->script.run = 1;
  dl->script.packetmode = 1;
  mp_linkInit(&dl->mp);

  dl->bundle = bundle;
  dl->next = NULL;

  memset(&dl->dial_timer, '\0', sizeof dl->dial_timer);

  dl->dial_tries = 0;
  dl->cfg.dial.max = 1;
  dl->cfg.dial.next_timeout = DIAL_NEXT_TIMEOUT;
  dl->cfg.dial.timeout = DIAL_TIMEOUT;

  dl->reconnect_tries = 0;
  dl->cfg.reconnect.max = 0;
  dl->cfg.reconnect.timeout = RECONNECT_TIMEOUT;

  dl->cfg.callback.opmask = 0;
  dl->cfg.cbcp.delay = 0;
  *dl->cfg.cbcp.phone = '\0';
  dl->cfg.cbcp.fsmretry = DEF_FSMRETRY;

  dl->name = strdup(name);
  peerid_Init(&dl->peer);
  dl->parent = &bundle->fsm;
  dl->fsmp.LayerStart = datalink_LayerStart;
  dl->fsmp.LayerUp = datalink_LayerUp;
  dl->fsmp.LayerDown = datalink_LayerDown;
  dl->fsmp.LayerFinish = datalink_LayerFinish;
  dl->fsmp.object = dl;

  if ((dl->physical = modem_Create(dl, type)) == NULL) {
    free(dl->name);
    free(dl);
    return NULL;
  }

  pap_Init(&dl->pap, dl->physical);
  chap_Init(&dl->chap, dl->physical);
  cbcp_Init(&dl->cbcp, dl->physical);
  chat_Init(&dl->chat, dl->physical, NULL, 1, NULL);

  log_Printf(LogPHASE, "%s: Created in %s state\n",
             dl->name, datalink_State(dl));

  return dl;
}

struct datalink *
datalink_Clone(struct datalink *odl, const char *name)
{
  struct datalink *dl;

  dl = (struct datalink *)malloc(sizeof(struct datalink));
  if (dl == NULL)
    return dl;

  dl->desc.type = DATALINK_DESCRIPTOR;
  dl->desc.UpdateSet = datalink_UpdateSet;
  dl->desc.IsSet = datalink_IsSet;
  dl->desc.Read = datalink_Read;
  dl->desc.Write = datalink_Write;

  dl->state = DATALINK_CLOSED;

  memcpy(&dl->cfg, &odl->cfg, sizeof dl->cfg);
  mp_linkInit(&dl->mp);
  *dl->phone.list = '\0';
  dl->phone.next = NULL;
  dl->phone.alt = NULL;
  dl->phone.chosen = "N/A";
  dl->bundle = odl->bundle;
  dl->next = NULL;
  memset(&dl->dial_timer, '\0', sizeof dl->dial_timer);
  dl->dial_tries = 0;
  dl->reconnect_tries = 0;
  dl->name = strdup(name);
  peerid_Init(&dl->peer);
  dl->parent = odl->parent;
  memcpy(&dl->fsmp, &odl->fsmp, sizeof dl->fsmp);
  dl->fsmp.object = dl;

  if ((dl->physical = modem_Create(dl, PHYS_INTERACTIVE)) == NULL) {
    free(dl->name);
    free(dl);
    return NULL;
  }
  pap_Init(&dl->pap, dl->physical);
  dl->pap.cfg.fsmretry = odl->pap.cfg.fsmretry;

  chap_Init(&dl->chap, dl->physical);
  dl->chap.auth.cfg.fsmretry = odl->chap.auth.cfg.fsmretry;

  memcpy(&dl->physical->cfg, &odl->physical->cfg, sizeof dl->physical->cfg);
  memcpy(&dl->physical->link.lcp.cfg, &odl->physical->link.lcp.cfg,
         sizeof dl->physical->link.lcp.cfg);
  memcpy(&dl->physical->link.ccp.cfg, &odl->physical->link.ccp.cfg,
         sizeof dl->physical->link.ccp.cfg);
  memcpy(&dl->physical->async.cfg, &odl->physical->async.cfg,
         sizeof dl->physical->async.cfg);

  cbcp_Init(&dl->cbcp, dl->physical);
  chat_Init(&dl->chat, dl->physical, NULL, 1, NULL);

  log_Printf(LogPHASE, "%s: Cloned in %s state\n",
             dl->name, datalink_State(dl));

  return dl;
}

struct datalink *
datalink_Destroy(struct datalink *dl)
{
  struct datalink *result;

  if (dl->state != DATALINK_CLOSED) {
    log_Printf(LogERROR, "Oops, destroying a datalink in state %s\n",
              datalink_State(dl));
    switch (dl->state) {
      case DATALINK_HANGUP:
      case DATALINK_DIAL:
      case DATALINK_LOGIN:
        chat_Destroy(&dl->chat);	/* Gotta blat the timers ! */
        break;
    }
  }

  timer_Stop(&dl->dial_timer);
  result = dl->next;
  modem_Destroy(dl->physical);
  free(dl->name);
  free(dl);

  return result;
}

void
datalink_Up(struct datalink *dl, int runscripts, int packetmode)
{
  if (dl->physical->type & (PHYS_DIRECT|PHYS_DEDICATED))
    /* Ignore scripts */
    runscripts = 0;

  switch (dl->state) {
    case DATALINK_CLOSED:
      if (bundle_Phase(dl->bundle) == PHASE_DEAD ||
          bundle_Phase(dl->bundle) == PHASE_TERMINATE)
        bundle_NewPhase(dl->bundle, PHASE_ESTABLISH);
      datalink_NewState(dl, DATALINK_OPENING);
      dl->reconnect_tries =
        dl->physical->type == PHYS_DIRECT ? 0 : dl->cfg.reconnect.max;
      dl->dial_tries = dl->cfg.dial.max;
      dl->script.run = runscripts;
      dl->script.packetmode = packetmode;
      break;

    case DATALINK_OPENING:
      if (!dl->script.run && runscripts)
        dl->script.run = 1;
      /* fall through */

    case DATALINK_DIAL:
    case DATALINK_LOGIN:
    case DATALINK_READY:
      if (!dl->script.packetmode && packetmode) {
        dl->script.packetmode = 1;
        if (dl->state == DATALINK_READY)
          datalink_LoginDone(dl);
      }
      break;
  }
}

void
datalink_Close(struct datalink *dl, int how)
{
  /* Please close */
  switch (dl->state) {
    case DATALINK_OPEN:
      peerid_Init(&dl->peer);
      fsm2initial(&dl->physical->link.ccp.fsm);
      /* fall through */

    case DATALINK_CBCP:
    case DATALINK_AUTH:
    case DATALINK_LCP:
      fsm_Close(&dl->physical->link.lcp.fsm);
      if (how != CLOSE_NORMAL) {
        dl->dial_tries = -1;
        dl->reconnect_tries = 0;
        if (how == CLOSE_LCP)
          dl->stayonline = 1;
      }
      break;

    default:
      datalink_ComeDown(dl, how);
  }
}

void
datalink_Down(struct datalink *dl, int how)
{
  /* Carrier is lost */
  switch (dl->state) {
    case DATALINK_OPEN:
      peerid_Init(&dl->peer);
      fsm2initial(&dl->physical->link.ccp.fsm);
      /* fall through */

    case DATALINK_CBCP:
    case DATALINK_AUTH:
    case DATALINK_LCP:
      fsm2initial(&dl->physical->link.lcp.fsm);
      /* fall through */

    default:
      datalink_ComeDown(dl, how);
  }
}

void
datalink_StayDown(struct datalink *dl)
{
  dl->reconnect_tries = 0;
}

void
datalink_DontHangup(struct datalink *dl)
{
  if (dl->state >= DATALINK_LCP)
    dl->stayonline = 1;
}

int
datalink_Show(struct cmdargs const *arg)
{
  prompt_Printf(arg->prompt, "Name: %s\n", arg->cx->name);
  prompt_Printf(arg->prompt, " State:              %s\n",
                datalink_State(arg->cx));
  prompt_Printf(arg->prompt, " CHAP Encryption:    %s\n",
                arg->cx->chap.using_MSChap ? "MSChap" : "MD5" );
  prompt_Printf(arg->prompt, " Peer name:          ");
  if (*arg->cx->peer.authname)
    prompt_Printf(arg->prompt, "%s\n", arg->cx->peer.authname);
  else if (arg->cx->state == DATALINK_OPEN)
    prompt_Printf(arg->prompt, "None requested\n");
  else
    prompt_Printf(arg->prompt, "N/A\n");
  prompt_Printf(arg->prompt, " Discriminator:      %s\n",
                mp_Enddisc(arg->cx->peer.enddisc.class,
                           arg->cx->peer.enddisc.address,
                           arg->cx->peer.enddisc.len));

  prompt_Printf(arg->prompt, "\nDefaults:\n");
  prompt_Printf(arg->prompt, " Phone List:         %s\n",
                arg->cx->cfg.phone.list);
  if (arg->cx->cfg.dial.max)
    prompt_Printf(arg->prompt, " Dial tries:         %d, delay ",
                  arg->cx->cfg.dial.max);
  else
    prompt_Printf(arg->prompt, " Dial tries:         infinite, delay ");
  if (arg->cx->cfg.dial.next_timeout > 0)
    prompt_Printf(arg->prompt, "%ds/", arg->cx->cfg.dial.next_timeout);
  else
    prompt_Printf(arg->prompt, "random/");
  if (arg->cx->cfg.dial.timeout > 0)
    prompt_Printf(arg->prompt, "%ds\n", arg->cx->cfg.dial.timeout);
  else
    prompt_Printf(arg->prompt, "random\n");
  prompt_Printf(arg->prompt, " Reconnect tries:    %d, delay ",
                arg->cx->cfg.reconnect.max);
  if (arg->cx->cfg.reconnect.timeout > 0)
    prompt_Printf(arg->prompt, "%ds\n", arg->cx->cfg.reconnect.timeout);
  else
    prompt_Printf(arg->prompt, "random\n");
  prompt_Printf(arg->prompt, " Callback %s ", arg->cx->physical->type ==
                PHYS_DIRECT ?  "accepted: " : "requested:");
  if (!arg->cx->cfg.callback.opmask)
    prompt_Printf(arg->prompt, "none\n");
  else {
    int comma = 0;

    if (arg->cx->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_NONE)) {
      prompt_Printf(arg->prompt, "none");
      comma = 1;
    }
    if (arg->cx->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_AUTH)) {
      prompt_Printf(arg->prompt, "%sauth", comma ? ", " : "");
      comma = 1;
    }
    if (arg->cx->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_E164)) {
      prompt_Printf(arg->prompt, "%sE.164", comma ? ", " : "");
      if (arg->cx->physical->type != PHYS_DIRECT)
        prompt_Printf(arg->prompt, " (%s)", arg->cx->cfg.callback.msg);
      comma = 1;
    }
    if (arg->cx->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_CBCP)) {
      prompt_Printf(arg->prompt, "%scbcp\n", comma ? ", " : "");
      prompt_Printf(arg->prompt, " CBCP:               delay: %ds\n",
                    arg->cx->cfg.cbcp.delay);
      prompt_Printf(arg->prompt, "                     phone: ");
      if (!strcmp(arg->cx->cfg.cbcp.phone, "*")) {
        if (arg->cx->physical->type & PHYS_DIRECT)
          prompt_Printf(arg->prompt, "Caller decides\n");
        else
          prompt_Printf(arg->prompt, "Dialback server decides\n");
      } else
        prompt_Printf(arg->prompt, "%s\n", arg->cx->cfg.cbcp.phone);
      prompt_Printf(arg->prompt, "                     timeout: %lds\n",
                    arg->cx->cfg.cbcp.fsmretry);
    } else
      prompt_Printf(arg->prompt, "\n");
  }

  prompt_Printf(arg->prompt, " Dial Script:        %s\n",
                arg->cx->cfg.script.dial);
  prompt_Printf(arg->prompt, " Login Script:       %s\n",
                arg->cx->cfg.script.login);
  prompt_Printf(arg->prompt, " Hangup Script:      %s\n",
                arg->cx->cfg.script.hangup);
  return 0;
}

int
datalink_SetReconnect(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn+2) {
    arg->cx->cfg.reconnect.timeout = atoi(arg->argv[arg->argn]);
    arg->cx->cfg.reconnect.max = atoi(arg->argv[arg->argn+1]);
    return 0;
  }
  return -1;
}

int
datalink_SetRedial(struct cmdargs const *arg)
{
  int timeout;
  int tries;
  char *dot;

  if (arg->argc == arg->argn+1 || arg->argc == arg->argn+2) {
    if (strncasecmp(arg->argv[arg->argn], "random", 6) == 0 &&
	(arg->argv[arg->argn][6] == '\0' || arg->argv[arg->argn][6] == '.')) {
      arg->cx->cfg.dial.timeout = -1;
      randinit();
    } else {
      timeout = atoi(arg->argv[arg->argn]);

      if (timeout >= 0)
	arg->cx->cfg.dial.timeout = timeout;
      else {
	log_Printf(LogWARN, "Invalid redial timeout\n");
	return -1;
      }
    }

    dot = strchr(arg->argv[arg->argn], '.');
    if (dot) {
      if (strcasecmp(++dot, "random") == 0) {
	arg->cx->cfg.dial.next_timeout = -1;
	randinit();
      } else {
	timeout = atoi(dot);
	if (timeout >= 0)
	  arg->cx->cfg.dial.next_timeout = timeout;
	else {
	  log_Printf(LogWARN, "Invalid next redial timeout\n");
	  return -1;
	}
      }
    } else
      /* Default next timeout */
      arg->cx->cfg.dial.next_timeout = DIAL_NEXT_TIMEOUT;

    if (arg->argc == arg->argn+2) {
      tries = atoi(arg->argv[arg->argn+1]);

      if (tries >= 0) {
	arg->cx->cfg.dial.max = tries;
      } else {
	log_Printf(LogWARN, "Invalid retry value\n");
	return 1;
      }
    }
    return 0;
  }
  return -1;
}

static const char *states[] = {
  "closed",
  "opening",
  "hangup",
  "dial",
  "login",
  "ready",
  "lcp",
  "auth",
  "cbcp",
  "open"
};

const char *
datalink_State(struct datalink *dl)
{
  if (dl->state < 0 || dl->state >= sizeof states / sizeof states[0])
    return "unknown";
  return states[dl->state];
}

static void
datalink_NewState(struct datalink *dl, int state)
{
  if (state != dl->state) {
    if (state >= 0 && state < sizeof states / sizeof states[0]) {
      log_Printf(LogPHASE, "%s: %s -> %s\n", dl->name, datalink_State(dl),
                 states[state]);
      dl->state = state;
    } else
      log_Printf(LogERROR, "%s: Can't enter state %d !\n", dl->name, state);
  }
}

struct datalink *
iov2datalink(struct bundle *bundle, struct iovec *iov, int *niov, int maxiov,
             int fd)
{
  struct datalink *dl, *cdl;
  u_int retry;
  char *oname;

  dl = (struct datalink *)iov[(*niov)++].iov_base;
  dl->name = iov[*niov].iov_base;

  if (dl->name[DATALINK_MAXNAME-1]) {
    dl->name[DATALINK_MAXNAME-1] = '\0';
    if (strlen(dl->name) == DATALINK_MAXNAME - 1)
      log_Printf(LogWARN, "Datalink name truncated to \"%s\"\n", dl->name);
  }

  /* Make sure the name is unique ! */
  oname = NULL;
  do {
    for (cdl = bundle->links; cdl; cdl = cdl->next)
      if (!strcasecmp(dl->name, cdl->name)) {
        if (oname)
          free(datalink_NextName(dl));
        else
          oname = datalink_NextName(dl);
        break;	/* Keep renaming 'till we have no conflicts */
      }
  } while (cdl);

  if (oname) {
    log_Printf(LogPHASE, "Rename link %s to %s\n", oname, dl->name);
    free(oname);
  } else {
    dl->name = strdup(dl->name);
    free(iov[*niov].iov_base);
  }
  (*niov)++;

  dl->desc.type = DATALINK_DESCRIPTOR;
  dl->desc.UpdateSet = datalink_UpdateSet;
  dl->desc.IsSet = datalink_IsSet;
  dl->desc.Read = datalink_Read;
  dl->desc.Write = datalink_Write;

  mp_linkInit(&dl->mp);
  *dl->phone.list = '\0';
  dl->phone.next = NULL;
  dl->phone.alt = NULL;
  dl->phone.chosen = "N/A";

  dl->bundle = bundle;
  dl->next = NULL;
  memset(&dl->dial_timer, '\0', sizeof dl->dial_timer);
  dl->dial_tries = 0;
  dl->reconnect_tries = 0;
  dl->parent = &bundle->fsm;
  dl->fsmp.LayerStart = datalink_LayerStart;
  dl->fsmp.LayerUp = datalink_LayerUp;
  dl->fsmp.LayerDown = datalink_LayerDown;
  dl->fsmp.LayerFinish = datalink_LayerFinish;
  dl->fsmp.object = dl;

  dl->physical = iov2modem(dl, iov, niov, maxiov, fd);

  if (!dl->physical) {
    free(dl->name);
    free(dl);
    dl = NULL;
  } else {
    retry = dl->pap.cfg.fsmretry;
    pap_Init(&dl->pap, dl->physical);
    dl->pap.cfg.fsmretry = retry;

    retry = dl->chap.auth.cfg.fsmretry;
    chap_Init(&dl->chap, dl->physical);
    dl->chap.auth.cfg.fsmretry = retry;

    cbcp_Init(&dl->cbcp, dl->physical);
    chat_Init(&dl->chat, dl->physical, NULL, 1, NULL);

    log_Printf(LogPHASE, "%s: Transferred in %s state\n",
              dl->name, datalink_State(dl));
  }

  return dl;
}

int
datalink2iov(struct datalink *dl, struct iovec *iov, int *niov, int maxiov,
             pid_t newpid)
{
  /* If `dl' is NULL, we're allocating before a Fromiov() */
  int link_fd;

  if (dl) {
    timer_Stop(&dl->dial_timer);
    /* The following is purely for the sake of paranoia */
    cbcp_Down(&dl->cbcp);
    timer_Stop(&dl->pap.authtimer);
    timer_Stop(&dl->chap.auth.authtimer);
  }

  if (*niov >= maxiov - 1) {
    log_Printf(LogERROR, "Toiov: No room for datalink !\n");
    if (dl) {
      free(dl->name);
      free(dl);
    }
    return -1;
  }

  iov[*niov].iov_base = dl ? dl : malloc(sizeof *dl);
  iov[(*niov)++].iov_len = sizeof *dl;
  iov[*niov].iov_base =
    dl ? realloc(dl->name, DATALINK_MAXNAME) : malloc(DATALINK_MAXNAME);
  iov[(*niov)++].iov_len = DATALINK_MAXNAME;

  link_fd = modem2iov(dl ? dl->physical : NULL, iov, niov, maxiov, newpid);

  if (link_fd == -1 && dl) {
    free(dl->name);
    free(dl);
  }

  return link_fd;
}

void
datalink_Rename(struct datalink *dl, const char *name)
{
  free(dl->name);
  dl->physical->link.name = dl->name = strdup(name);
}

char *
datalink_NextName(struct datalink *dl)
{
  int f, n;
  char *name, *oname;

  n = strlen(dl->name);
  name = (char *)malloc(n+3);
  for (f = n - 1; f >= 0; f--)
    if (!isdigit(dl->name[f]))
      break;
  n = sprintf(name, "%.*s-", dl->name[f] == '-' ? f : f + 1, dl->name);
  sprintf(name + n, "%d", atoi(dl->name + f + 1) + 1);
  oname = dl->name;
  dl->name = name;
  /* our physical link name isn't updated (it probably isn't created yet) */
  return oname;
}

int
datalink_SetMode(struct datalink *dl, int mode)
{
  if (!physical_SetMode(dl->physical, mode))
    return 0;
  if (dl->physical->type & (PHYS_DIRECT|PHYS_DEDICATED))
    dl->script.run = 0;
  if (dl->physical->type == PHYS_DIRECT)
    dl->reconnect_tries = 0;
  if (mode & (PHYS_DDIAL|PHYS_BACKGROUND) && dl->state <= DATALINK_READY)
    datalink_Up(dl, 1, 1);
  return 1;
}
