/*
 *			PPP CHAP Module
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
 * $Id: chap.c,v 1.8 1999/02/18 19:46:19 brian Exp $
 *
 *	TODO:
 */
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_DES
#include <md4.h>
#endif
#include <md5.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcpproto.h"
#include "lcp.h"
#include "lqr.h"
#include "hdlc.h"
#include "auth.h"
#include "async.h"
#include "throughput.h"
#include "descriptor.h"
#include "chap.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "chat.h"
#include "cbcp.h"
#include "command.h"
#include "datalink.h"
#ifdef HAVE_DES
#include "chap_ms.h"
#endif

static const char *chapcodes[] = {
  "???", "CHALLENGE", "RESPONSE", "SUCCESS", "FAILURE"
};
#define MAXCHAPCODE (sizeof chapcodes / sizeof chapcodes[0] - 1)

static void
ChapOutput(struct physical *physical, u_int code, u_int id,
	   const u_char *ptr, int count, const char *text)
{
  int plen;
  struct fsmheader lh;
  struct mbuf *bp;

  plen = sizeof(struct fsmheader) + count;
  lh.code = code;
  lh.id = id;
  lh.length = htons(plen);
  bp = mbuf_Alloc(plen, MB_FSM);
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  if (count)
    memcpy(MBUF_CTOP(bp) + sizeof(struct fsmheader), ptr, count);
  log_DumpBp(LogDEBUG, "ChapOutput", bp);
  if (text == NULL)
    log_Printf(LogPHASE, "Chap Output: %s\n", chapcodes[code]);
  else
    log_Printf(LogPHASE, "Chap Output: %s (%s)\n", chapcodes[code], text);
  hdlc_Output(&physical->link, PRI_LINK, PROTO_CHAP, bp);
}

static char *
chap_BuildAnswer(char *name, char *key, u_char id, char *challenge, u_char type
#ifdef HAVE_DES
                 , int lanman
#endif
                )
{
  char *result, *digest;
  size_t nlen, klen;

  nlen = strlen(name);
  klen = strlen(key);

#ifdef HAVE_DES
  if (type == 0x80) {
    char expkey[AUTHLEN << 2];
    MD4_CTX MD4context;
    int f;

    if ((result = malloc(1 + nlen + MS_CHAP_RESPONSE_LEN)) == NULL)
      return result;

    digest = result;					/* the response */
    *digest++ = MS_CHAP_RESPONSE_LEN;			/* 49 */
    memcpy(digest + MS_CHAP_RESPONSE_LEN, name, nlen);
    if (lanman) {
      memset(digest + 24, '\0', 25);
      mschap_LANMan(digest, challenge + 1, key);	/* LANMan response */
    } else {
      memset(digest, '\0', 25);
      digest += 24;

      for (f = 0; f < klen; f++) {
        expkey[2*f] = key[f];
        expkey[2*f+1] = '\0';
      }
      /*
       *           -----------
       * expkey = | k\0e\0y\0 |
       *           -----------
       */
      MD4Init(&MD4context);
      MD4Update(&MD4context, expkey, klen << 1);
      MD4Final(digest, &MD4context);

      /*
       *           ---- -------- ---------------- ------- ------
       * result = | 49 | LANMan | 16 byte digest | 9 * ? | name |
       *           ---- -------- ---------------- ------- ------
       */
      mschap_NT(digest, challenge + 1);
    }
    /*
     *           ---- -------- ------------- ----- ------
     *          |    |  struct MS_ChapResponse24  |      |
     * result = | 49 | LANMan  |  NT digest | 0/1 | name |
     *           ---- -------- ------------- ----- ------
     * where only one of LANMan & NT digest are set.
     */
  } else
#endif
  if ((result = malloc(nlen + 17)) != NULL) {
    /* Normal MD5 stuff */
    MD5_CTX MD5context;

    digest = result;
    *digest++ = 16;				/* value size */

    MD5Init(&MD5context);
    MD5Update(&MD5context, &id, 1);
    MD5Update(&MD5context, key, klen);
    MD5Update(&MD5context, challenge + 1, *challenge);
    MD5Final(digest, &MD5context);

    memcpy(digest + 16, name, nlen);
    /*
     *           ---- -------- ------
     * result = | 16 | digest | name |
     *           ---- -------- ------
     */
  }

  return result;
}

static void
chap_StartChild(struct chap *chap, char *prog, const char *name)
{
  char *argv[MAXARGS], *nargv[MAXARGS];
  int argc, fd;
  int in[2], out[2];

  if (chap->child.fd != -1) {
    log_Printf(LogWARN, "Chap: %s: Program already running\n", prog);
    return;
  }

  if (pipe(in) == -1) {
    log_Printf(LogERROR, "Chap: pipe: %s\n", strerror(errno));
    return;
  }

  if (pipe(out) == -1) {
    log_Printf(LogERROR, "Chap: pipe: %s\n", strerror(errno));
    close(in[0]);
    close(in[1]);
    return;
  }

  switch ((chap->child.pid = fork())) {
    case -1:
      log_Printf(LogERROR, "Chap: fork: %s\n", strerror(errno));
      close(in[0]);
      close(in[1]);
      close(out[0]);
      close(out[1]);
      chap->child.pid = 0;
      return;

    case 0:
      timer_TermService();
      close(in[1]);
      close(out[0]);
      if (out[1] == STDIN_FILENO) {
        fd = dup(out[1]);
        close(out[1]);
        out[1] = fd;
      }
      dup2(in[0], STDIN_FILENO);
      dup2(out[1], STDOUT_FILENO);
      if ((fd = open(_PATH_DEVNULL, O_RDWR)) == -1) {
        log_Printf(LogALERT, "Chap: Failed to open %s: %s\n",
                  _PATH_DEVNULL, strerror(errno));
        exit(1);
      }
      dup2(fd, STDERR_FILENO);
      fcntl(3, F_SETFD, 1);		/* Set close-on-exec flag */

      setuid(geteuid());
      argc = command_Interpret(prog, strlen(prog), argv);
      command_Expand(nargv, argc, (char const *const *)argv,
                     chap->auth.physical->dl->bundle, 0);
      execvp(nargv[0], nargv);

      log_Printf(LogWARN, "exec() of %s failed: %s\n",
                nargv[0], strerror(errno));
      exit(255);

    default:
      close(in[0]);
      close(out[1]);
      chap->child.fd = out[0];
      chap->child.buf.len = 0;
      write(in[1], chap->auth.in.name, strlen(chap->auth.in.name));
      write(in[1], "\n", 1);
      write(in[1], chap->challenge + 1, *chap->challenge);
      write(in[1], "\n", 1);
      write(in[1], name, strlen(name));
      write(in[1], "\n", 1);
      close(in[1]);
      break;
  }
}

static void
chap_Cleanup(struct chap *chap, int sig)
{
  if (chap->child.pid) {
    int status;

    close(chap->child.fd);
    chap->child.fd = -1;
    if (sig)
      kill(chap->child.pid, SIGTERM);
    chap->child.pid = 0;
    chap->child.buf.len = 0;

    if (wait(&status) == -1)
      log_Printf(LogERROR, "Chap: wait: %s\n", strerror(errno));
    else if (WIFSIGNALED(status))
      log_Printf(LogWARN, "Chap: Child received signal %d\n", WTERMSIG(status));
    else if (WIFEXITED(status) && WEXITSTATUS(status))
      log_Printf(LogERROR, "Chap: Child exited %d\n", WEXITSTATUS(status));
  }
  *chap->challenge = 0;
#ifdef HAVE_DES
  chap->peertries = 0;
#endif
}

static void
chap_Respond(struct chap *chap, char *name, char *key, u_char type
#ifdef HAVE_DES
             , int lm
#endif
            )
{
  u_char *ans;

  ans = chap_BuildAnswer(name, key, chap->auth.id, chap->challenge, type
#ifdef HAVE_DES
                         , lm
#endif
                        );

  if (ans) {
    ChapOutput(chap->auth.physical, CHAP_RESPONSE, chap->auth.id,
               ans, *ans + 1 + strlen(name), name);
#ifdef HAVE_DES
    chap->NTRespSent = !lm;
#endif
    free(ans);
  } else
    ChapOutput(chap->auth.physical, CHAP_FAILURE, chap->auth.id,
               "Out of memory!", 14, NULL);
}

static int
chap_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct chap *chap = descriptor2chap(d);

  if (r && chap && chap->child.fd != -1) {
    FD_SET(chap->child.fd, r);
    if (*n < chap->child.fd + 1)
      *n = chap->child.fd + 1;
    log_Printf(LogTIMER, "Chap: fdset(r) %d\n", chap->child.fd);
    return 1;
  }

  return 0;
}

static int
chap_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct chap *chap = descriptor2chap(d);

  return chap && chap->child.fd != -1 && FD_ISSET(chap->child.fd, fdset);
}

static void
chap_Read(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct chap *chap = descriptor2chap(d);
  int got;

  got = read(chap->child.fd, chap->child.buf.ptr + chap->child.buf.len,
             sizeof chap->child.buf.ptr - chap->child.buf.len - 1);
  if (got == -1) {
    log_Printf(LogERROR, "Chap: Read: %s\n", strerror(errno));
    chap_Cleanup(chap, SIGTERM);
  } else if (got == 0) {
    log_Printf(LogWARN, "Chap: Read: Child terminated connection\n");
    chap_Cleanup(chap, SIGTERM);
  } else {
    char *name, *key, *end;

    chap->child.buf.len += got;
    chap->child.buf.ptr[chap->child.buf.len] = '\0';
    name = chap->child.buf.ptr;
    name += strspn(name, " \t");
    if ((key = strchr(name, '\n')) == NULL)
      end = NULL;
    else
      end = strchr(++key, '\n');

    if (end == NULL) {
      if (chap->child.buf.len == sizeof chap->child.buf.ptr - 1) {
        log_Printf(LogWARN, "Chap: Read: Input buffer overflow\n");
        chap_Cleanup(chap, SIGTERM);
      }
    } else {
#ifdef HAVE_DES
      int lanman = chap->auth.physical->link.lcp.his_authtype == 0x80 &&
                   ((chap->NTRespSent &&
                     IsAccepted(chap->auth.physical->link.lcp.cfg.chap80lm)) ||
                    !IsAccepted(chap->auth.physical->link.lcp.cfg.chap80nt));
#endif

      while (end >= name && strchr(" \t\r\n", *end))
        *end-- = '\0';
      end = key - 1;
      while (end >= name && strchr(" \t\r\n", *end))
        *end-- = '\0';
      key += strspn(key, " \t");

      chap_Respond(chap, name, key, chap->auth.physical->link.lcp.his_authtype
#ifdef HAVE_DES
                   , lanman
#endif
                  );
      chap_Cleanup(chap, 0);
    }
  }
}

static int
chap_Write(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  /* We never want to write here ! */
  log_Printf(LogALERT, "chap_Write: Internal error: Bad call !\n");
  return 0;
}

static void
chap_Challenge(struct authinfo *authp)
{
  struct chap *chap = auth2chap(authp);
  int len, i;
  char *cp;

  len = strlen(authp->physical->dl->bundle->cfg.auth.name);

  if (!*chap->challenge) {
    randinit();
    cp = chap->challenge;

#ifndef NORADIUS
    if (*authp->physical->dl->bundle->radius.cfg.file) {
      /* For radius, our challenge is 16 readable NUL terminated bytes :*/
      *cp++ = 16;
      for (i = 0; i < 16; i++)
        *cp++ = (random() % 10) + '0';
    } else
#endif
    {
#ifdef HAVE_DES
      if (authp->physical->link.lcp.want_authtype == 0x80)
        *cp++ = 8;	/* MS does 8 byte callenges :-/ */
      else
#endif
        *cp++ = random() % (CHAPCHALLENGELEN-16) + 16;
      for (i = 0; i < *chap->challenge; i++)
        *cp++ = random() & 0xff;
    }
    memcpy(cp, authp->physical->dl->bundle->cfg.auth.name, len);
  }
  ChapOutput(authp->physical, CHAP_CHALLENGE, authp->id, chap->challenge,
	     1 + *chap->challenge + len, NULL);
}

static void
chap_Success(struct authinfo *authp)
{
  datalink_GotAuthname(authp->physical->dl, authp->in.name);
  ChapOutput(authp->physical, CHAP_SUCCESS, authp->id, "Welcome!!", 10, NULL);
  authp->physical->link.lcp.auth_ineed = 0;
  if (Enabled(authp->physical->dl->bundle, OPT_UTMP))
    physical_Login(authp->physical, authp->in.name);

  if (authp->physical->link.lcp.auth_iwait == 0)
    /*
     * Either I didn't need to authenticate, or I've already been
     * told that I got the answer right.
     */
    datalink_AuthOk(authp->physical->dl);
}

static void
chap_Failure(struct authinfo *authp)
{
  ChapOutput(authp->physical, CHAP_FAILURE, authp->id, "Invalid!!", 9, NULL);
  datalink_AuthNotOk(authp->physical->dl);
}

static int
chap_Cmp(u_char type, char *myans, int mylen, char *hisans, int hislen
#ifdef HAVE_DES
         , int lm
#endif
        )
{
  if (mylen != hislen)
    return 0;
#ifdef HAVE_DES
  else if (type == 0x80) {
    int off = lm ? 0 : 24;

    if (memcmp(myans + off, hisans + off, 24))
      return 0;
  }
#endif
  else if (memcmp(myans, hisans, mylen))
    return 0;

  return 1;
}

#ifdef HAVE_DES
static int
chap_HaveAnotherGo(struct chap *chap)
{
  if (++chap->peertries < 3) {
    /* Give the peer another shot */
    *chap->challenge = '\0';
    chap_Challenge(&chap->auth);
    return 1;
  }

  return 0;
}
#endif

void
chap_Init(struct chap *chap, struct physical *p)
{
  chap->desc.type = CHAP_DESCRIPTOR;
  chap->desc.UpdateSet = chap_UpdateSet;
  chap->desc.IsSet = chap_IsSet;
  chap->desc.Read = chap_Read;
  chap->desc.Write = chap_Write;
  chap->child.pid = 0;
  chap->child.fd = -1;
  auth_Init(&chap->auth, p, chap_Challenge, chap_Success, chap_Failure);
  *chap->challenge = 0;
#ifdef HAVE_DES
  chap->NTRespSent = 0;
  chap->peertries = 0;
#endif
}

void
chap_ReInit(struct chap *chap)
{
  chap_Cleanup(chap, SIGTERM);
}

void
chap_Input(struct physical *p, struct mbuf *bp)
{
  struct chap *chap = &p->dl->chap;
  char *name, *key, *ans;
  int len, nlen;
  u_char alen;
#ifdef HAVE_DES
  int lanman;
#endif

  if ((bp = auth_ReadHeader(&chap->auth, bp)) == NULL &&
      ntohs(chap->auth.in.hdr.length) == 0)
    log_Printf(LogWARN, "Chap Input: Truncated header !\n");
  else if (chap->auth.in.hdr.code == 0 || chap->auth.in.hdr.code > MAXCHAPCODE)
    log_Printf(LogPHASE, "Chap Input: %d: Bad CHAP code !\n",
               chap->auth.in.hdr.code);
  else {
    len = mbuf_Length(bp);
    ans = NULL;

    if (chap->auth.in.hdr.code != CHAP_CHALLENGE &&
        chap->auth.id != chap->auth.in.hdr.id &&
        Enabled(p->dl->bundle, OPT_IDCHECK)) {
      /* Wrong conversation dude ! */
      log_Printf(LogPHASE, "Chap Input: %s dropped (got id %d, not %d)\n",
                 chapcodes[chap->auth.in.hdr.code], chap->auth.in.hdr.id,
                 chap->auth.id);
      mbuf_Free(bp);
      return;
    }
    chap->auth.id = chap->auth.in.hdr.id;	/* We respond with this id */

#ifdef HAVE_DES
    lanman = 0;
#endif
    switch (chap->auth.in.hdr.code) {
      case CHAP_CHALLENGE:
        bp = mbuf_Read(bp, &alen, 1);
        len -= alen + 1;
        if (len < 0) {
          log_Printf(LogERROR, "Chap Input: Truncated challenge !\n");
          mbuf_Free(bp);
          return;
        }
        *chap->challenge = alen;
        bp = mbuf_Read(bp, chap->challenge + 1, alen);
        bp = auth_ReadName(&chap->auth, bp, len);
#ifdef HAVE_DES
        lanman = p->link.lcp.his_authtype == 0x80 &&
                 ((chap->NTRespSent && IsAccepted(p->link.lcp.cfg.chap80lm)) ||
                  !IsAccepted(p->link.lcp.cfg.chap80nt));
#endif
        break;

      case CHAP_RESPONSE:
        auth_StopTimer(&chap->auth);
        bp = mbuf_Read(bp, &alen, 1);
        len -= alen + 1;
        if (len < 0) {
          log_Printf(LogERROR, "Chap Input: Truncated response !\n");
          mbuf_Free(bp);
          return;
        }
        if ((ans = malloc(alen + 2)) == NULL) {
          log_Printf(LogERROR, "Chap Input: Out of memory !\n");
          mbuf_Free(bp);
          return;
        }
        *ans = chap->auth.id;
        bp = mbuf_Read(bp, ans + 1, alen);
        ans[alen+1] = '\0';
        bp = auth_ReadName(&chap->auth, bp, len);
#ifdef HAVE_DES
        lanman = alen == 49 && ans[alen] == 0;
#endif
        break;

      case CHAP_SUCCESS:
      case CHAP_FAILURE:
        /* chap->auth.in.name is already set up at CHALLENGE time */
        if ((ans = malloc(len + 1)) == NULL) {
          log_Printf(LogERROR, "Chap Input: Out of memory !\n");
          mbuf_Free(bp);
          return;
        }
        bp = mbuf_Read(bp, ans, len);
        ans[len] = '\0';
        break;
    }

    switch (chap->auth.in.hdr.code) {
      case CHAP_CHALLENGE:
      case CHAP_RESPONSE:
        if (*chap->auth.in.name)
          log_Printf(LogPHASE, "Chap Input: %s (%d bytes from %s%s)\n",
                     chapcodes[chap->auth.in.hdr.code], alen,
                     chap->auth.in.name,
#ifdef HAVE_DES
                     lanman && chap->auth.in.hdr.code == CHAP_RESPONSE ?
                     " - lanman" :
#endif
                     "");
        else
          log_Printf(LogPHASE, "Chap Input: %s (%d bytes%s)\n",
                     chapcodes[chap->auth.in.hdr.code], alen,
#ifdef HAVE_DES
                     lanman && chap->auth.in.hdr.code == CHAP_RESPONSE ?
                     " - lanman" :
#endif
                     "");
        break;

      case CHAP_SUCCESS:
      case CHAP_FAILURE:
        if (*ans)
          log_Printf(LogPHASE, "Chap Input: %s (%s)\n",
                     chapcodes[chap->auth.in.hdr.code], ans);
        else
          log_Printf(LogPHASE, "Chap Input: %s\n",
                     chapcodes[chap->auth.in.hdr.code]);
        break;
    }

    switch (chap->auth.in.hdr.code) {
      case CHAP_CHALLENGE:
        if (*p->dl->bundle->cfg.auth.key == '!')
          chap_StartChild(chap, p->dl->bundle->cfg.auth.key + 1,
                          p->dl->bundle->cfg.auth.name);
        else
          chap_Respond(chap, p->dl->bundle->cfg.auth.name,
                       p->dl->bundle->cfg.auth.key, p->link.lcp.his_authtype
#ifdef HAVE_DES
                       , lanman
#endif
                      );
        break;

      case CHAP_RESPONSE:
        name = chap->auth.in.name;
        nlen = strlen(name);
#ifndef NORADIUS
        if (*p->dl->bundle->radius.cfg.file) {
          chap->challenge[*chap->challenge+1] = '\0';
          radius_Authenticate(&p->dl->bundle->radius, &chap->auth,
                              chap->auth.in.name, ans, chap->challenge + 1);
        } else
#endif
        {
          key = auth_GetSecret(p->dl->bundle, name, nlen, p);
          if (key) {
            char *myans;
#ifdef HAVE_DES
            if (lanman && !IsEnabled(p->link.lcp.cfg.chap80lm)) {
              log_Printf(LogPHASE, "Auth failure: LANMan not enabled\n");
              if (chap_HaveAnotherGo(chap))
                break;
              key = NULL;
            } else if (!lanman && !IsEnabled(p->link.lcp.cfg.chap80nt) &&
                       p->link.lcp.want_authtype == 0x80) {
              log_Printf(LogPHASE, "Auth failure: mschap not enabled\n");
              if (chap_HaveAnotherGo(chap))
                break;
              key = NULL;
            } else
#endif
            {
              myans = chap_BuildAnswer(name, key, chap->auth.id,
                                       chap->challenge,
                                       p->link.lcp.want_authtype
#ifdef HAVE_DES
                                       , lanman
#endif
                                      );
              if (myans == NULL)
                key = NULL;
              else {
                if (!chap_Cmp(p->link.lcp.want_authtype, myans + 1, *myans,
                              ans + 1, alen
#ifdef HAVE_DES
                              , lanman
#endif
                             ))
                  key = NULL;
                free(myans);
              }
            }
          }

          if (key)
            chap_Success(&chap->auth);
          else
            chap_Failure(&chap->auth);
        }

        break;

      case CHAP_SUCCESS:
        if (p->link.lcp.auth_iwait == PROTO_CHAP) {
          p->link.lcp.auth_iwait = 0;
          if (p->link.lcp.auth_ineed == 0)
            /*
             * We've succeeded in our ``login''
             * If we're not expecting  the peer to authenticate (or he already
             * has), proceed to network phase.
             */
            datalink_AuthOk(p->dl);
        }
        break;

      case CHAP_FAILURE:
        datalink_AuthNotOk(p->dl);
        break;
    }
    free(ans);
  }

  mbuf_Free(bp);
}
