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
 * $Id: main.c,v 1.19 1998/06/27 12:06:46 brian Exp $
 *
 *	TODO:
 *		o Add commands for traffic summary, version display, etc.
 *		o Add signal handler for misc controls.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "modem.h"
#include "os.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "ipcp.h"
#include "loadalias.h"
#include "vars.h"
#include "auth.h"
#include "filter.h"
#include "systems.h"
#include "ip.h"
#include "sig.h"
#include "server.h"
#include "main.h"
#include "vjcomp.h"
#include "async.h"
#include "pathnames.h"
#include "tun.h"
#include "route.h"

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define	O_NONBLOCK O_NDELAY
#endif
#endif

int TermMode = 0;
int tunno = 0;

static struct termios oldtio;	/* Original tty mode */
static struct termios comtio;	/* Command level tty mode */
static pid_t BGPid = 0;
static char pid_filename[MAXPATHLEN];
static int dial_up;

static void DoLoop(void);
static void TerminalStop(int);
static const char *ex_desc(int);

static void
TtyInit(int DontWantInt)
{
  struct termios newtio;
  int stat;

  stat = fcntl(netfd, F_GETFL, 0);
  if (stat > 0) {
    stat |= O_NONBLOCK;
    (void) fcntl(netfd, F_SETFL, stat);
  }
  newtio = oldtio;
  newtio.c_lflag &= ~(ECHO | ISIG | ICANON);
  newtio.c_iflag = 0;
  newtio.c_oflag &= ~OPOST;
  newtio.c_cc[VEOF] = _POSIX_VDISABLE;
  if (DontWantInt)
    newtio.c_cc[VINTR] = _POSIX_VDISABLE;
  newtio.c_cc[VMIN] = 1;
  newtio.c_cc[VTIME] = 0;
  newtio.c_cflag |= CS8;
  tcsetattr(netfd, TCSANOW, &newtio);
  comtio = newtio;
}

/*
 *  Set tty into command mode. We allow canonical input and echo processing.
 */
void
TtyCommandMode(int prompt)
{
  struct termios newtio;
  int stat;

  if (!(mode & MODE_INTER))
    return;
  tcgetattr(netfd, &newtio);
  newtio.c_lflag |= (ECHO | ISIG | ICANON);
  newtio.c_iflag = oldtio.c_iflag;
  newtio.c_oflag |= OPOST;
  tcsetattr(netfd, TCSADRAIN, &newtio);
  stat = fcntl(netfd, F_GETFL, 0);
  if (stat > 0) {
    stat |= O_NONBLOCK;
    (void) fcntl(netfd, F_SETFL, stat);
  }
  TermMode = 0;
  if (prompt)
    Prompt();
}

/*
 * Set tty into terminal mode which is used while we invoke term command.
 */
void
TtyTermMode()
{
  int stat;

  tcsetattr(netfd, TCSADRAIN, &comtio);
  stat = fcntl(netfd, F_GETFL, 0);
  if (stat > 0) {
    stat &= ~O_NONBLOCK;
    (void) fcntl(netfd, F_SETFL, stat);
  }
  TermMode = 1;
}

void
TtyOldMode()
{
  int stat;

  stat = fcntl(netfd, F_GETFL, 0);
  if (stat > 0) {
    stat &= ~O_NONBLOCK;
    (void) fcntl(netfd, F_SETFL, stat);
  }
  tcsetattr(netfd, TCSADRAIN, &oldtio);
}

void
Cleanup(int excode)
{
  DropClient(1);
  ServerClose();
  OsInterfaceDown(1);
  HangupModem(1);
  nointr_sleep(1);
  DeleteIfRoutes(1);
  ID0unlink(pid_filename);
  if (mode & MODE_BACKGROUND && BGFiledes[1] != -1) {
    char c = EX_ERRDEAD;

    if (write(BGFiledes[1], &c, 1) == 1)
      LogPrintf(LogPHASE, "Parent notified of failure.\n");
    else
      LogPrintf(LogPHASE, "Failed to notify parent of failure.\n");
    close(BGFiledes[1]);
  }
  LogPrintf(LogPHASE, "PPP Terminated (%s).\n", ex_desc(excode));
  TtyOldMode();
  LogClose();

  exit(excode);
}

static void
CloseConnection(int signo)
{
  /* NOTE, these are manual, we've done a setsid() */
  pending_signal(SIGINT, SIG_IGN);
  LogPrintf(LogPHASE, "Caught signal %d, abort connection\n", signo);
  reconnectState = RECON_FALSE;
  reconnectCount = 0;
  DownConnection();
  dial_up = 0;
  pending_signal(SIGINT, CloseConnection);
}

static void
CloseSession(int signo)
{
  if (BGPid) {
    kill(BGPid, SIGINT);
    exit(EX_TERM);
  }
  LogPrintf(LogPHASE, "Signal %d, terminate.\n", signo);
  reconnect(RECON_FALSE);
  LcpClose();
  Cleanup(EX_TERM);
}

static void
TerminalCont(int signo)
{
  pending_signal(SIGCONT, SIG_DFL);
  pending_signal(SIGTSTP, TerminalStop);
  TtyCommandMode(getpgrp() == tcgetpgrp(netfd));
}

static void
TerminalStop(int signo)
{
  pending_signal(SIGCONT, TerminalCont);
  TtyOldMode();
  pending_signal(SIGTSTP, SIG_DFL);
  kill(getpid(), signo);
}

static void
SetUpServer(int signo)
{
  int res;

  VarHaveLocalAuthKey = 0;
  LocalAuthInit();
  if ((res = ServerTcpOpen(SERVER_PORT + tunno)) != 0)
    LogPrintf(LogERROR, "SIGUSR1: Failed %d to open port %d\n",
	      res, SERVER_PORT + tunno);
}

static void
BringDownServer(int signo)
{
  VarHaveLocalAuthKey = 0;
  LocalAuthInit();
  ServerClose();
}

static const char *
ex_desc(int ex)
{
  static char num[12];
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
          " [system]\n");
  exit(EX_START);
}

static char *
ProcessArgs(int argc, char **argv)
{
  int optc;
  char *cp;

  optc = 0;
  mode = MODE_INTER;
  while (argc > 0 && **argv == '-') {
    cp = *argv + 1;
    if (strcmp(cp, "auto") == 0) {
      mode |= MODE_AUTO;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "background") == 0) {
      mode |= MODE_BACKGROUND;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "direct") == 0) {
      mode |= MODE_DIRECT;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "dedicated") == 0) {
      mode |= MODE_DEDICATED;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "ddial") == 0) {
      mode |= MODE_DDIAL;
      mode &= ~MODE_INTER;
#ifndef NOALIAS
    } else if (strcmp(cp, "alias") == 0) {
      if (loadAliasHandlers(&VarAliasHandlers) == 0)
	mode |= MODE_ALIAS;
      else
	LogPrintf(LogWARN, "Cannot load alias library\n");
      optc--;			/* this option isn't exclusive */
#endif
    } else
      Usage();
    optc++;
    argv++;
    argc--;
  }
  if (argc > 1) {
    fprintf(stderr, "specify only one system label.\n");
    exit(EX_START);
  }

  if (optc > 1) {
    fprintf(stderr, "specify only one mode.\n");
    exit(EX_START);
  }

  return argc == 1 ? *argv : NULL;	/* Don't SetLabel yet ! */
}

int
main(int argc, char **argv)
{
  FILE *lockfile;
  char *name, *label;
  int nfds;

  nfds = getdtablesize();
  if (nfds >= FD_SETSIZE)
    /*
     * If we've got loads of file descriptors, make sure they're all
     * closed.  If they aren't, we may end up with a seg fault when our
     * `fd_set's get too big when select()ing !
     */
    while (--nfds > 2)
      close(nfds);

  VarTerm = 0;
  name = strrchr(argv[0], '/');
  LogOpen(name ? name + 1 : argv[0]);

  tcgetattr(STDIN_FILENO, &oldtio);	/* Save original tty mode */

  argc--;
  argv++;
  label = ProcessArgs(argc, argv);
  if (!(mode & MODE_DIRECT))
    VarTerm = stdout;

  ID0init();
  if (ID0realuid() != 0) {
    char conf[200], *ptr;

    snprintf(conf, sizeof conf, "%s/%s", _PATH_PPP, CONFFILE);
    do {
      if (!access(conf, W_OK)) {
        LogPrintf(LogALERT, "ppp: Access violation: Please protect %s\n", conf);
        return -1;
      }
      ptr = conf + strlen(conf)-2;
      while (ptr > conf && *ptr != '/')
        *ptr-- = '\0';
    } while (ptr >= conf);
  }

  if (!ValidSystem(label)) {
    fprintf(stderr, "You may not use ppp in this mode with this label\n");
    if (mode & MODE_DIRECT) {
      const char *l;
      l = label ? label : "default";
      LogPrintf(LogWARN, "Label %s rejected -direct connection\n", l);
    }
    LogClose();
    return 1;
  }

  if (!GetShortHost())
    return 1;
  IsInteractive(1);
  IpcpDefAddress();

  if (mode & MODE_INTER)
    VarLocalAuth = LOCAL_AUTH;

  if (SelectSystem("default", CONFFILE) < 0 && VarTerm)
    fprintf(VarTerm, "Warning: No default entry is given in config file.\n");

  if (OpenTunnel(&tunno) < 0) {
    LogPrintf(LogWARN, "OpenTunnel: %s\n", strerror(errno));
    return EX_START;
  }
  CleanInterface(IfDevName);
  if ((mode & MODE_OUTGOING_DAEMON) && !(mode & MODE_DEDICATED))
    if (label == NULL) {
      if (VarTerm)
	fprintf(VarTerm, "Destination system must be specified in"
		" auto, background or ddial mode.\n");
      return EX_START;
    }

  pending_signal(SIGHUP, CloseSession);
  pending_signal(SIGTERM, CloseSession);
  pending_signal(SIGINT, CloseConnection);
  pending_signal(SIGQUIT, CloseSession);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SIGALRM
  pending_signal(SIGALRM, SIG_IGN);
#endif
  if (mode & MODE_INTER) {
#ifdef SIGTSTP
    pending_signal(SIGTSTP, TerminalStop);
#endif
#ifdef SIGTTIN
    pending_signal(SIGTTIN, TerminalStop);
#endif
#ifdef SIGTTOU
    pending_signal(SIGTTOU, SIG_IGN);
#endif
  }
  if (!(mode & MODE_INTER)) {
#ifdef SIGUSR1
    pending_signal(SIGUSR1, SetUpServer);
#endif
#ifdef SIGUSR2
    pending_signal(SIGUSR2, BringDownServer);
#endif
  }

  if (label) {
    if (SelectSystem(label, CONFFILE) < 0) {
      LogPrintf(LogWARN, "Destination system %s not found in conf file.\n",
                GetLabel());
      Cleanup(EX_START);
    }
    /*
     * We don't SetLabel() 'till now in case SelectSystem() has an
     * embeded load "otherlabel" command.
     */
    SetLabel(label);
    if (mode & MODE_OUTGOING_DAEMON &&
	DefHisAddress.ipaddr.s_addr == INADDR_ANY) {
      LogPrintf(LogWARN, "You must \"set ifaddr\" in label %s for"
		" auto, background or ddial mode.\n", label);
      Cleanup(EX_START);
    }
  }

  if (mode & MODE_DAEMON) {
    if (mode & MODE_BACKGROUND) {
      if (pipe(BGFiledes)) {
	LogPrintf(LogERROR, "pipe: %s\n", strerror(errno));
	Cleanup(EX_SOCK);
      }
    }

    if (!(mode & MODE_DIRECT)) {
      pid_t bgpid;

      bgpid = fork();
      if (bgpid == -1) {
	LogPrintf(LogERROR, "fork: %s\n", strerror(errno));
	Cleanup(EX_SOCK);
      }
      if (bgpid) {
	char c = EX_NORMAL;

	if (mode & MODE_BACKGROUND) {
	  /* Wait for our child to close its pipe before we exit. */
	  BGPid = bgpid;
	  close(BGFiledes[1]);
	  if (read(BGFiledes[0], &c, 1) != 1) {
	    fprintf(VarTerm, "Child exit, no status.\n");
	    LogPrintf(LogPHASE, "Parent: Child exit, no status.\n");
	  } else if (c == EX_NORMAL) {
	    fprintf(VarTerm, "PPP enabled.\n");
	    LogPrintf(LogPHASE, "Parent: PPP enabled.\n");
	  } else {
	    fprintf(VarTerm, "Child failed (%s).\n", ex_desc((int) c));
	    LogPrintf(LogPHASE, "Parent: Child failed (%s).\n",
		      ex_desc((int) c));
	  }
	  close(BGFiledes[0]);
	}
	return c;
      } else if (mode & MODE_BACKGROUND)
	close(BGFiledes[0]);
    }

    VarTerm = 0;		/* We know it's currently stdout */
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (mode & MODE_DIRECT)
      /* STDIN_FILENO gets used by OpenModem in DIRECT mode */
      TtyInit(1);
    else if (mode & MODE_DAEMON) {
      setsid();
      close(STDIN_FILENO);
    }
  } else {
    close(STDIN_FILENO);
    if ((netfd = open(_PATH_TTY, O_RDONLY)) < 0) {
      fprintf(stderr, "Cannot open %s for intput !\n", _PATH_TTY);
      return 2;
    }
    close(STDERR_FILENO);
    TtyInit(0);
    TtyCommandMode(1);
  }

  snprintf(pid_filename, sizeof pid_filename, "%stun%d.pid",
           _PATH_VARRUN, tunno);
  lockfile = ID0fopen(pid_filename, "w");
  if (lockfile != NULL) {
    fprintf(lockfile, "%d\n", (int) getpid());
    fclose(lockfile);
  }
#ifndef RELEASE_CRUNCH
  else
    LogPrintf(LogALERT, "Warning: Can't create %s: %s\n",
              pid_filename, strerror(errno));
#endif

  LogPrintf(LogPHASE, "PPP Started.\n");


  do
    DoLoop();
  while (mode & MODE_DEDICATED);

  Cleanup(EX_DONE);
  return 0;
}

/*
 *  Turn into packet mode, where we speak PPP.
 */
void
PacketMode(int delay)
{
  if (RawModem() < 0) {
    LogPrintf(LogWARN, "PacketMode: Not connected.\n");
    return;
  }
  AsyncInit();
  VjInit(15);
  LcpInit();
  IpcpInit();
  CcpInit();
  LcpUp();

  LcpOpen(delay);
  if (mode & MODE_INTER)
    TtyCommandMode(1);
  if (VarTerm) {
    fprintf(VarTerm, "Packet mode.\n");
    aft_cmd = 1;
  }
}

static void
ShowHelp(void)
{
  fprintf(stderr, "The following commands are available:\r\n");
  fprintf(stderr, " ~p\tEnter Packet mode\r\n");
  fprintf(stderr, " ~-\tDecrease log level\r\n");
  fprintf(stderr, " ~+\tIncrease log level\r\n");
  fprintf(stderr, " ~t\tShow timers (only in \"log debug\" mode)\r\n");
  fprintf(stderr, " ~m\tShow memory map (only in \"log debug\" mode)\r\n");
  fprintf(stderr, " ~.\tTerminate program\r\n");
  fprintf(stderr, " ~?\tThis help\r\n");
}

static void
ReadTty(void)
{
  int n;
  char ch;
  static int ttystate;
  char linebuff[LINE_LEN];

  LogPrintf(LogDEBUG, "termode = %d, netfd = %d, mode = %d\n",
	    TermMode, netfd, mode);
  if (!TermMode) {
    n = read(netfd, linebuff, sizeof linebuff - 1);
    if (n > 0) {
      aft_cmd = 1;
      if (linebuff[n-1] == '\n')
        linebuff[--n] = '\0';
      else
        linebuff[n] = '\0';
      if (n)
        DecodeCommand(linebuff, n, IsInteractive(0) ? NULL : "Client");
      Prompt();
    } else if (n <= 0) {
      LogPrintf(LogPHASE, "Client connection closed.\n");
      DropClient(0);
    }
    return;
  }

  /*
   * We are in terminal mode, decode special sequences
   */
  n = read(netfd, &ch, 1);
  LogPrintf(LogDEBUG, "Got %d bytes (reading from the terminal)\n", n);

  if (n > 0) {
    switch (ttystate) {
    case 0:
      if (ch == '~')
	ttystate++;
      else
	write(modem, &ch, n);
      break;
    case 1:
      switch (ch) {
      case '?':
	ShowHelp();
	break;
      case 'p':

	/*
	 * XXX: Should check carrier.
	 */
	if (LcpFsm.state <= ST_CLOSED)
	  PacketMode(0);
	break;
      case '.':
	TermMode = 1;
	aft_cmd = 1;
	TtyCommandMode(1);
	break;
      case 't':
	if (LogIsKept(LogDEBUG)) {
	  ShowTimers();
	  break;
	}
      case 'm':
	if (LogIsKept(LogDEBUG)) {
	  ShowMemMap(NULL);
	  break;
	}
      default:
	if (write(modem, &ch, n) < 0)
	  LogPrintf(LogERROR, "error writing to modem.\n");
	break;
      }
      ttystate = 0;
      break;
    }
  }
}


/*
 *  Here, we'll try to detect HDLC frame
 */

static const char *FrameHeaders[] = {
  "\176\377\003\300\041",
  "\176\377\175\043\300\041",
  "\176\177\175\043\100\041",
  "\176\175\337\175\043\300\041",
  "\176\175\137\175\043\100\041",
  NULL,
};

static const u_char *
HdlcDetect(u_char * cp, int n)
{
  const char *ptr, *fp, **hp;

  cp[n] = '\0';			/* be sure to null terminated */
  ptr = NULL;
  for (hp = FrameHeaders; *hp; hp++) {
    fp = *hp;
    if (DEV_IS_SYNC)
      fp++;
    ptr = strstr((char *) cp, fp);
    if (ptr)
      break;
  }
  return ((const u_char *) ptr);
}

static struct pppTimer RedialTimer;

static void
RedialTimeout(void *v)
{
  StopTimer(&RedialTimer);
  LogPrintf(LogPHASE, "Redialing timer expired.\n");
}

static void
StartRedialTimer(int Timeout)
{
  StopTimer(&RedialTimer);

  if (Timeout) {
    RedialTimer.state = TIMER_STOPPED;

    if (Timeout > 0)
      RedialTimer.load = Timeout * SECTICKS;
    else
      RedialTimer.load = (random() % REDIAL_PERIOD) * SECTICKS;

    LogPrintf(LogPHASE, "Enter pause (%d) for redialing.\n",
	      RedialTimer.load / SECTICKS);

    RedialTimer.func = RedialTimeout;
    StartTimer(&RedialTimer);
  }
}

#define IN_SIZE sizeof(struct sockaddr_in)
#define UN_SIZE sizeof(struct sockaddr_in)
#define ADDRSZ (IN_SIZE > UN_SIZE ? IN_SIZE : UN_SIZE)

static void
DoLoop(void)
{
  fd_set rfds, wfds, efds;
  int pri, i, n, wfd, nfds;
  char hisaddr[ADDRSZ];
  struct sockaddr *sa = (struct sockaddr *)hisaddr;
  struct sockaddr_in *sockin = (struct sockaddr_in *)hisaddr;
  struct timeval timeout, *tp;
  int ssize = ADDRSZ;
  const u_char *cp;
  int tries;
  int qlen;
  int res;
  struct tun_data tun;
#define rbuff tun.data

  if (mode & MODE_DIRECT) {
    LogPrintf(LogDEBUG, "Opening modem\n");
    if (OpenModem() < 0)
      return;
    LogPrintf(LogPHASE, "Packet mode enabled\n");
    PacketMode(VarOpenMode);
  } else if (mode & MODE_DEDICATED) {
    if (modem < 0)
      while (OpenModem() < 0)
	nointr_sleep(VarReconnectTimer);
  }
  fflush(VarTerm);

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  reconnectState = RECON_UNKNOWN;

  if (mode & MODE_BACKGROUND)
    dial_up = 1;		/* Bring the line up */
  else
    dial_up = 0;		/* XXXX */
  tries = 0;
  for (;;) {
    nfds = 0;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    /*
     * If the link is down and we're in DDIAL mode, bring it back up.
     */
    if (mode & MODE_DDIAL && LcpFsm.state <= ST_CLOSED)
      dial_up = 1;

    /*
     * If we lost carrier and want to re-establish the connection due to the
     * "set reconnect" value, we'd better bring the line back up.
     */
    if (LcpFsm.state <= ST_CLOSED) {
      if (!dial_up && reconnectState == RECON_TRUE) {
	if (++reconnectCount <= VarReconnectTries) {
	  LogPrintf(LogPHASE, "Connection lost, re-establish (%d/%d)\n",
		    reconnectCount, VarReconnectTries);
	  StartRedialTimer(VarReconnectTimer);
	  dial_up = 1;
	} else {
	  if (VarReconnectTries)
	    LogPrintf(LogPHASE, "Connection lost, maximum (%d) times\n",
		      VarReconnectTries);
	  reconnectCount = 0;
	  if (mode & MODE_BACKGROUND)
	    Cleanup(EX_DEAD);
	}
	reconnectState = RECON_ENVOKED;
      } else if (mode & MODE_DEDICATED)
        PacketMode(VarOpenMode);
    }

    /*
     * If Ip packet for output is enqueued and require dial up, Just do it!
     */
    if (dial_up && RedialTimer.state != TIMER_RUNNING) {
      LogPrintf(LogDEBUG, "going to dial: modem = %d\n", modem);
      if (OpenModem() < 0) {
	tries++;
	if (!(mode & MODE_DDIAL) && VarDialTries)
	  LogPrintf(LogCHAT, "Failed to open modem (attempt %u of %d)\n",
		    tries, VarDialTries);
	else
	  LogPrintf(LogCHAT, "Failed to open modem (attempt %u)\n", tries);

	if (!(mode & MODE_DDIAL) && VarDialTries && tries >= VarDialTries) {
	  if (mode & MODE_BACKGROUND)
	    Cleanup(EX_DIAL);	/* Can't get the modem */
	  dial_up = 0;
	  reconnectState = RECON_UNKNOWN;
	  reconnectCount = 0;
	  tries = 0;
	} else
	  StartRedialTimer(VarRedialTimeout);
      } else {
	tries++;		/* Tries are per number, not per list of
				 * numbers. */
	if (!(mode & MODE_DDIAL) && VarDialTries)
	  LogPrintf(LogCHAT, "Dial attempt %u of %d\n", tries, VarDialTries);
	else
	  LogPrintf(LogCHAT, "Dial attempt %u\n", tries);

	if ((res = DialModem()) == EX_DONE) {
	  ModemTimeout(NULL);
	  PacketMode(VarOpenMode);
	  dial_up = 0;
	  reconnectState = RECON_UNKNOWN;
	  tries = 0;
	} else {
	  if (mode & MODE_BACKGROUND) {
	    if (VarNextPhone == NULL || res == EX_SIG)
	      Cleanup(EX_DIAL);	/* Tried all numbers - no luck */
	    else
	      /* Try all numbers in background mode */
	      StartRedialTimer(VarRedialNextTimeout);
	  } else if (!(mode & MODE_DDIAL) &&
		     ((VarDialTries && tries >= VarDialTries) ||
		      res == EX_SIG)) {
	    /* I give up !  Can't get through :( */
	    StartRedialTimer(VarRedialTimeout);
	    dial_up = 0;
	    reconnectState = RECON_UNKNOWN;
	    reconnectCount = 0;
	    tries = 0;
	  } else if (VarNextPhone == NULL)
	    /* Dial failed. Keep quite during redial wait period. */
	    StartRedialTimer(VarRedialTimeout);
	  else
	    StartRedialTimer(VarRedialNextTimeout);
	}
      }
    }
    qlen = ModemQlen();

    if (qlen == 0) {
      IpStartOutput();
      qlen = ModemQlen();
    }

#ifdef SIGALRM
    handle_signals();
#endif

    if (modem >= 0) {
      if (modem + 1 > nfds)
	nfds = modem + 1;
      FD_SET(modem, &rfds);
      FD_SET(modem, &efds);
      if (qlen > 0) {
	FD_SET(modem, &wfds);
      }
    }
    if (server >= 0) {
      if (server + 1 > nfds)
	nfds = server + 1;
      FD_SET(server, &rfds);
    }

#ifndef SIGALRM
    /*
     * *** IMPORTANT ***
     * CPU is serviced every TICKUNIT micro seconds. This value must be chosen
     * with great care. If this values is too big, it results in loss of
     * characters from the modem and poor response.  If this value is too
     * small, ppp eats too much CPU time.
     */
    usleep(TICKUNIT);
    TimerService();
#endif

    /* If there are aren't many packets queued, look for some more. */
    if (qlen < 20 && tun_in >= 0) {
      if (tun_in + 1 > nfds)
	nfds = tun_in + 1;
      FD_SET(tun_in, &rfds);
    }
    if (netfd >= 0) {
      if (netfd + 1 > nfds)
	nfds = netfd + 1;
      FD_SET(netfd, &rfds);
      FD_SET(netfd, &efds);
    }
#ifndef SIGALRM

    /*
     * Normally, select() will not block because modem is writable. In AUTO
     * mode, select will block until we find packet from tun
     */
    tp = (RedialTimer.state == TIMER_RUNNING) ? &timeout : NULL;
    i = select(nfds, &rfds, &wfds, &efds, tp);
#else

    /*
     * When SIGALRM timer is running, a select function will be return -1 and
     * EINTR after a Time Service signal hundler is done.  If the redial
     * timer is not running and we are trying to dial, poll with a 0 value
     * timer.
     */
    tp = (dial_up && RedialTimer.state != TIMER_RUNNING) ? &timeout : NULL;
    i = select(nfds, &rfds, &wfds, &efds, tp);
#endif

    if (i == 0) {
      continue;
    }
    if (i < 0) {
      if (errno == EINTR) {
	handle_signals();
	continue;
      }
      LogPrintf(LogERROR, "DoLoop: select(): %s\n", strerror(errno));
      break;
    }
    if ((netfd >= 0 && FD_ISSET(netfd, &efds)) || (modem >= 0 && FD_ISSET(modem, &efds))) {
      LogPrintf(LogALERT, "Exception detected.\n");
      break;
    }
    if (server >= 0 && FD_ISSET(server, &rfds)) {
      wfd = accept(server, sa, &ssize);
      if (wfd < 0) {
	LogPrintf(LogERROR, "DoLoop: accept(): %s\n", strerror(errno));
	continue;
      }
      switch (sa->sa_family) {
        case AF_LOCAL:
          LogPrintf(LogPHASE, "Connected to local client.\n");
          break;
        case AF_INET:
          if (ntohs(sockin->sin_port) < 1024) {
            LogPrintf(LogALERT, "Rejected client connection from %s:%u"
                      "(invalid port number) !\n",
                      inet_ntoa(sockin->sin_addr), ntohs(sockin->sin_port));
	    close(wfd);
	    continue;
          }
          LogPrintf(LogPHASE, "Connected to client from %s:%u\n",
                    inet_ntoa(sockin->sin_addr), sockin->sin_port);
          break;
        default:
	  write(wfd, "Unrecognised access !\n", 22);
	  close(wfd);
	  continue;
      }
      if (netfd >= 0) {
	write(wfd, "Connection already in use.\n", 27);
	close(wfd);
	continue;
      }
      netfd = wfd;
      VarTerm = fdopen(netfd, "a+");
      LocalAuthInit();
      IsInteractive(1);
      Prompt();
    }
    if (netfd >= 0 && FD_ISSET(netfd, &rfds))
      /* something to read from tty */
      ReadTty();
    if (modem >= 0 && FD_ISSET(modem, &wfds)) {
      /* ready to write into modem */
      ModemStartOutput(modem);
      if (modem < 0)
        dial_up = 1;
    }
    if (modem >= 0 && FD_ISSET(modem, &rfds)) {
      /* something to read from modem */
      if (LcpFsm.state <= ST_CLOSED)
	nointr_usleep(10000);
      n = read(modem, rbuff, sizeof rbuff);
      if ((mode & MODE_DIRECT) && n <= 0) {
	DownConnection();
      } else
	LogDumpBuff(LogASYNC, "ReadFromModem", rbuff, n);

      if (LcpFsm.state <= ST_CLOSED) {
	/*
	 * In dedicated mode, we just discard input until LCP is started.
	 */
	if (!(mode & MODE_DEDICATED)) {
	  cp = HdlcDetect(rbuff, n);
	  if (cp) {
	    /*
	     * LCP packet is detected. Turn ourselves into packet mode.
	     */
	    if (cp != rbuff) {
	      write(modem, rbuff, cp - rbuff);
	      write(modem, "\r\n", 2);
	    }
	    PacketMode(0);
	  } else
	    write(fileno(VarTerm), rbuff, n);
	}
      } else {
	if (n > 0)
	  AsyncInput(rbuff, n);
      }
    }
    if (tun_in >= 0 && FD_ISSET(tun_in, &rfds)) {	/* something to read
							 * from tun */
      n = read(tun_in, &tun, sizeof tun);
      if (n < 0) {
	LogPrintf(LogERROR, "read from tun: %s\n", strerror(errno));
	continue;
      }
      n -= sizeof tun - sizeof tun.data;
      if (n <= 0) {
	LogPrintf(LogERROR, "read from tun: Only %d bytes read\n", n);
	continue;
      }
      if (!tun_check_header(tun, AF_INET))
          continue;
      if (((struct ip *) rbuff)->ip_dst.s_addr == IpcpInfo.want_ipaddr.s_addr) {
	/* we've been asked to send something addressed *to* us :( */
	if (VarLoopback) {
	  pri = PacketCheck(rbuff, n, FL_IN);
	  if (pri >= 0) {
	    struct mbuf *bp;

#ifndef NOALIAS
	    if (mode & MODE_ALIAS) {
	      VarPacketAliasIn(rbuff, sizeof rbuff);
	      n = ntohs(((struct ip *) rbuff)->ip_len);
	    }
#endif
	    bp = mballoc(n, MB_IPIN);
	    memcpy(MBUF_CTOP(bp), rbuff, n);
	    IpInput(bp);
	    LogPrintf(LogDEBUG, "Looped back packet addressed to myself\n");
	  }
	  continue;
	} else
	  LogPrintf(LogDEBUG, "Oops - forwarding packet addressed to myself\n");
      }

      /*
       * Process on-demand dialup. Output packets are queued within tunnel
       * device until IPCP is opened.
       */
      if (LcpFsm.state <= ST_CLOSED && (mode & MODE_AUTO) &&
	  (pri = PacketCheck(rbuff, n, FL_DIAL)) >= 0)
        dial_up = 1;

      pri = PacketCheck(rbuff, n, FL_OUT);
      if (pri >= 0) {
#ifndef NOALIAS
	if (mode & MODE_ALIAS) {
	  VarPacketAliasOut(rbuff, sizeof rbuff);
	  n = ntohs(((struct ip *) rbuff)->ip_len);
	}
#endif
	IpEnqueue(pri, rbuff, n);
      }
    }
  }
  LogPrintf(LogDEBUG, "Job (DoLoop) done.\n");
}
