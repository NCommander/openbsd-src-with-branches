/*
 *			User Process PPP
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
 * $Id: main.c,v 1.149 1999/02/02 09:35:29 brian Exp $
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
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#ifndef NOALIAS
#ifdef __OpenBSD__
#include "alias.h"
#else
#include <alias.h>
#endif
#endif
#include "probe.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "link.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "auth.h"
#include "systems.h"
#include "sig.h"
#include "main.h"
#include "server.h"
#include "prompt.h"
#include "chat.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "iface.h"

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define	O_NONBLOCK O_NDELAY
#endif
#endif

static void DoLoop(struct bundle *);
static void TerminalStop(int);
static const char *ex_desc(int);

static struct bundle *SignalBundle;
static struct prompt *SignalPrompt;

void
Cleanup(int excode)
{
  SignalBundle->CleaningUp = 1;
  bundle_Close(SignalBundle, NULL, CLOSE_STAYDOWN);
}

void
AbortProgram(int excode)
{
  server_Close(SignalBundle);
  log_Printf(LogPHASE, "PPP Terminated (%s).\n", ex_desc(excode));
  bundle_Close(SignalBundle, NULL, CLOSE_STAYDOWN);
  bundle_Destroy(SignalBundle);
  log_Close();
  exit(excode);
}

static void
CloseConnection(int signo)
{
  /* NOTE, these are manual, we've done a setsid() */
  sig_signal(SIGINT, SIG_IGN);
  log_Printf(LogPHASE, "Caught signal %d, abort connection(s)\n", signo);
  bundle_Down(SignalBundle, CLOSE_STAYDOWN);
  sig_signal(SIGINT, CloseConnection);
}

static void
CloseSession(int signo)
{
  log_Printf(LogPHASE, "Signal %d, terminate.\n", signo);
  Cleanup(EX_TERM);
}

static pid_t BGPid = 0;

static void
KillChild(int signo)
{
  log_Printf(LogPHASE, "Parent: Signal %d\n", signo);
  kill(BGPid, SIGINT);
}

static void
TerminalCont(int signo)
{
  signal(SIGCONT, SIG_DFL);
  prompt_Continue(SignalPrompt);
}

static void
TerminalStop(int signo)
{
  prompt_Suspend(SignalPrompt);
  signal(SIGCONT, TerminalCont);
  raise(SIGSTOP);
}

static void
BringDownServer(int signo)
{
  /* Drops all child prompts too ! */
  server_Close(SignalBundle);
}

static const char *
ex_desc(int ex)
{
  static char num[12];		/* Used immediately if returned */
  static const char *desc[] = {
    "normal", "start", "sock", "modem", "dial", "dead", "done",
    "reboot", "errdead", "hangup", "term", "nodial", "nologin"
  };

  if (ex >= 0 && ex < sizeof desc / sizeof *desc)
    return desc[ex];
  snprintf(num, sizeof num, "%d", ex);
  return num;
}

static void
Usage(void)
{
  fprintf(stderr,
	  "Usage: ppp [-auto | -background | -direct | -dedicated | -ddial ]"
#ifndef NOALIAS
          " [ -alias ]"
#endif
          " [system ...]\n");
  exit(EX_START);
}

static int
ProcessArgs(int argc, char **argv, int *mode, int *alias)
{
  int optc, newmode, arg;
  char *cp;

  optc = 0;
  *mode = PHYS_INTERACTIVE;
  *alias = 0;
  for (arg = 1; arg < argc && *argv[arg] == '-'; arg++, optc++) {
    cp = argv[arg] + 1;
    newmode = Nam2mode(cp);
    switch (newmode) {
      case PHYS_NONE:
        if (strcmp(cp, "alias") == 0) {
#ifdef NOALIAS
          log_Printf(LogWARN, "Cannot load alias library (compiled out)\n");
#else
          *alias = 1;
#endif
          optc--;			/* this option isn't exclusive */
        } else
          Usage();
        break;

      case PHYS_ALL:
        Usage();
        break;

      default:
        *mode = newmode;
    }
  }

  if (optc > 1) {
    fprintf(stderr, "You may specify only one mode.\n");
    exit(EX_START);
  }

  if (*mode == PHYS_AUTO && arg == argc) {
    fprintf(stderr, "A system must be specified in auto mode.\n");
    exit(EX_START);
  }

  return arg;		/* Don't SetLabel yet ! */
}

static void
CheckLabel(const char *label, struct prompt *prompt, int mode)
{
  const char *err;

  if ((err = system_IsValid(label, prompt, mode)) != NULL) {
    fprintf(stderr, "%s: %s\n", label, err);
    if (mode == PHYS_DIRECT)
      log_Printf(LogWARN, "Label %s rejected -direct connection: %s\n",
                 label, err);
    log_Close();
    exit(1);
  }
}


int
main(int argc, char **argv)
{
  char *name;
  const char *lastlabel;
  int nfds, mode, alias, label, arg;
  struct bundle *bundle;
  struct prompt *prompt;

  nfds = getdtablesize();
  if (nfds >= FD_SETSIZE)
    /*
     * If we've got loads of file descriptors, make sure they're all
     * closed.  If they aren't, we may end up with a seg fault when our
     * `fd_set's get too big when select()ing !
     */
    while (--nfds > 2)
      close(nfds);

  name = strrchr(argv[0], '/');
  log_Open(name ? name + 1 : argv[0]);

#ifndef NOALIAS
  PacketAliasInit();
#endif
  label = ProcessArgs(argc, argv, &mode, &alias);

#ifdef __FreeBSD__
  /*
   * A FreeBSD hack to dodge a bug in the tty driver that drops output
   * occasionally.... I must find the real reason some time.  To display
   * the dodgy behaviour, comment out this bit, make yourself a large
   * routing table and then run ppp in interactive mode.  The `show route'
   * command will drop chunks of data !!!
   */
  if (mode == PHYS_INTERACTIVE) {
    close(STDIN_FILENO);
    if (open(_PATH_TTY, O_RDONLY) != STDIN_FILENO) {
      fprintf(stderr, "Cannot open %s for input !\n", _PATH_TTY);
      return 2;
    }
  }
#endif

  /* Allow output for the moment (except in direct mode) */
  if (mode == PHYS_DIRECT)
    prompt = NULL;
  else
    SignalPrompt = prompt = prompt_Create(NULL, NULL, PROMPT_STD);

  ID0init();
  if (ID0realuid() != 0) {
    char conf[200], *ptr;

    snprintf(conf, sizeof conf, "%s/%s", _PATH_PPP, CONFFILE);
    do {
      if (!access(conf, W_OK)) {
        log_Printf(LogALERT, "ppp: Access violation: Please protect %s\n",
                   conf);
        return -1;
      }
      ptr = conf + strlen(conf)-2;
      while (ptr > conf && *ptr != '/')
        *ptr-- = '\0';
    } while (ptr >= conf);
  }

  if (label < argc)
    for (arg = label; arg < argc; arg++)
      CheckLabel(argv[arg], prompt, mode);
  else
    CheckLabel("default", prompt, mode);

  prompt_Printf(prompt, "Working in %s mode\n", mode2Nam(mode));

  if ((bundle = bundle_Create(TUN_PREFIX, mode, (const char **)argv)) == NULL) {
    log_Printf(LogWARN, "bundle_Create: %s\n", strerror(errno));
    return EX_START;
  }

  /* NOTE:  We may now have changed argv[1] via a ``set proctitle'' */

  if (prompt) {
    prompt->bundle = bundle;	/* couldn't do it earlier */
    prompt_Printf(prompt, "Using interface: %s\n", bundle->iface->name);
  }
  SignalBundle = bundle;
  bundle->AliasEnabled = alias;
  if (alias)
    bundle->cfg.opt |= OPT_IFACEALIAS;

  if (system_Select(bundle, "default", CONFFILE, prompt, NULL) < 0)
    prompt_Printf(prompt, "Warning: No default entry found in config file.\n");

  sig_signal(SIGHUP, CloseSession);
  sig_signal(SIGTERM, CloseSession);
  sig_signal(SIGINT, CloseConnection);
  sig_signal(SIGQUIT, CloseSession);
  sig_signal(SIGALRM, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  if (mode == PHYS_INTERACTIVE)
    sig_signal(SIGTSTP, TerminalStop);

  sig_signal(SIGUSR2, BringDownServer);

  lastlabel = argc == 2 ? bundle->argv1 : argv[argc - 1];
  for (arg = label; arg < argc; arg++) {
    /* In case we use LABEL or ``set enddisc label'' */
    bundle_SetLabel(bundle, lastlabel);
    system_Select(bundle, arg == 1 ? bundle->argv1 : argv[arg],
                  CONFFILE, prompt, NULL);
  }

  if (label < argc)
    /* In case the last label did a ``load'' */
    bundle_SetLabel(bundle, lastlabel);

  if (mode == PHYS_AUTO &&
      bundle->ncp.ipcp.cfg.peer_range.ipaddr.s_addr == INADDR_ANY) {
    prompt_Printf(prompt, "You must ``set ifaddr'' with a peer address "
                  "in auto mode.\n");
    AbortProgram(EX_START);
  }

  if (mode != PHYS_INTERACTIVE) {
    if (mode != PHYS_DIRECT) {
      int bgpipe[2];
      pid_t bgpid;

      if (mode == PHYS_BACKGROUND && pipe(bgpipe)) {
        log_Printf(LogERROR, "pipe: %s\n", strerror(errno));
	AbortProgram(EX_SOCK);
      }

      bgpid = fork();
      if (bgpid == -1) {
	log_Printf(LogERROR, "fork: %s\n", strerror(errno));
	AbortProgram(EX_SOCK);
      }

      if (bgpid) {
	char c = EX_NORMAL;

	if (mode == PHYS_BACKGROUND) {
	  close(bgpipe[1]);
	  BGPid = bgpid;
          /* If we get a signal, kill the child */
          signal(SIGHUP, KillChild);
          signal(SIGTERM, KillChild);
          signal(SIGINT, KillChild);
          signal(SIGQUIT, KillChild);

	  /* Wait for our child to close its pipe before we exit */
	  if (read(bgpipe[0], &c, 1) != 1) {
	    prompt_Printf(prompt, "Child exit, no status.\n");
	    log_Printf(LogPHASE, "Parent: Child exit, no status.\n");
	  } else if (c == EX_NORMAL) {
	    prompt_Printf(prompt, "PPP enabled.\n");
	    log_Printf(LogPHASE, "Parent: PPP enabled.\n");
	  } else {
	    prompt_Printf(prompt, "Child failed (%s).\n", ex_desc((int) c));
	    log_Printf(LogPHASE, "Parent: Child failed (%s).\n",
		      ex_desc((int) c));
	  }
	  close(bgpipe[0]);
	}
	return c;
      } else if (mode == PHYS_BACKGROUND) {
	close(bgpipe[0]);
        bundle->notify.fd = bgpipe[1];
      }

      bundle_LockTun(bundle);	/* we have a new pid */

      /* -auto, -dedicated, -ddial & -background */
      prompt_Destroy(prompt, 0);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
      close(STDIN_FILENO);
      setsid();
    } else {
      /* -direct: STDIN_FILENO gets used by modem_Open */
      prompt_TtyInit(NULL);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
    }
  } else {
    /* Interactive mode */
    close(STDERR_FILENO);
    prompt_TtyInit(prompt);
    prompt_TtyCommandMode(prompt);
    prompt_Required(prompt);
  }

  log_Printf(LogPHASE, "PPP Started (%s mode).\n", mode2Nam(mode));
  DoLoop(bundle);
  AbortProgram(EX_NORMAL);

  return EX_NORMAL;
}

static void
DoLoop(struct bundle *bundle)
{
  fd_set rfds, wfds, efds;
  int i, nfds, nothing_done;
  struct probe probe;

  probe_Init(&probe);

  do {
    nfds = 0;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    /* All our datalinks, the tun device and the MP socket */
    descriptor_UpdateSet(&bundle->desc, &rfds, &wfds, &efds, &nfds);

    /* All our prompts and the diagnostic socket */
    descriptor_UpdateSet(&server.desc, &rfds, NULL, NULL, &nfds);

    if (bundle_IsDead(bundle))
      /* Don't select - we'll be here forever */
      break;

    i = select(nfds, &rfds, &wfds, &efds, NULL);

    if (i < 0 && errno != EINTR) {
      log_Printf(LogERROR, "DoLoop: select(): %s\n", strerror(errno));
      if (log_IsKept(LogTIMER)) {
        struct timeval t;

        for (i = 0; i <= nfds; i++) {
          if (FD_ISSET(i, &rfds)) {
            log_Printf(LogTIMER, "Read set contains %d\n", i);
            FD_CLR(i, &rfds);
            t.tv_sec = t.tv_usec = 0;
            if (select(nfds, &rfds, &wfds, &efds, &t) != -1) {
              log_Printf(LogTIMER, "The culprit !\n");
              break;
            }
          }
          if (FD_ISSET(i, &wfds)) {
            log_Printf(LogTIMER, "Write set contains %d\n", i);
            FD_CLR(i, &wfds);
            t.tv_sec = t.tv_usec = 0;
            if (select(nfds, &rfds, &wfds, &efds, &t) != -1) {
              log_Printf(LogTIMER, "The culprit !\n");
              break;
            }
          }
          if (FD_ISSET(i, &efds)) {
            log_Printf(LogTIMER, "Error set contains %d\n", i);
            FD_CLR(i, &efds);
            t.tv_sec = t.tv_usec = 0;
            if (select(nfds, &rfds, &wfds, &efds, &t) != -1) {
              log_Printf(LogTIMER, "The culprit !\n");
              break;
            }
          }
        }
      }
      break;
    }

    log_Printf(LogTIMER, "Select returns %d\n", i);

    sig_Handle();

    if (i <= 0)
      continue;

    for (i = 0; i <= nfds; i++)
      if (FD_ISSET(i, &efds)) {
        log_Printf(LogPHASE, "Exception detected on descriptor %d\n", i);
        /* We deal gracefully with link descriptor exceptions */
        if (!bundle_Exception(bundle, i)) {
          log_Printf(LogERROR, "Exception cannot be handled !\n");
          break;
        }
      }

    if (i <= nfds)
      break;

    nothing_done = 1;

    if (descriptor_IsSet(&server.desc, &rfds)) {
      descriptor_Read(&server.desc, bundle, &rfds);
      nothing_done = 0;
    }

    if (descriptor_IsSet(&bundle->desc, &rfds)) {
      descriptor_Read(&bundle->desc, bundle, &rfds);
      nothing_done = 0;
    }

    if (descriptor_IsSet(&bundle->desc, &wfds))
      if (!descriptor_Write(&bundle->desc, bundle, &wfds) && nothing_done) {
        /*
         * This is disasterous.  The OS has told us that something is
         * writable, and all our write()s have failed.  Rather than
         * going back immediately to do our UpdateSet()s and select(),
         * we sleep for a bit to avoid gobbling up all cpu time.
         */
        struct timeval t;

        t.tv_sec = 0;
        t.tv_usec = 100000;
        select(0, NULL, NULL, NULL, &t);
      }

  } while (bundle_CleanDatalinks(bundle), !bundle_IsDead(bundle));

  log_Printf(LogDEBUG, "DoLoop done.\n");
}
