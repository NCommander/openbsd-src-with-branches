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
 *	$Id: bundle.h,v 1.21 1999/01/28 01:56:30 brian Exp $
 */

#define	PHASE_DEAD		0	/* Link is dead */
#define	PHASE_ESTABLISH		1	/* Establishing link */
#define	PHASE_AUTHENTICATE	2	/* Being authenticated */
#define	PHASE_NETWORK		3	/* We're alive ! */
#define	PHASE_TERMINATE		4	/* Terminating link */

/* cfg.opt bit settings */
#define OPT_IDCHECK	0x0001
#define OPT_IFACEALIAS	0x0002
#define OPT_LOOPBACK	0x0004
#define OPT_PASSWDAUTH	0x0008
#define OPT_PROXY	0x0010
#define OPT_PROXYALL	0x0020
#define OPT_SROUTES	0x0040
#define OPT_THROUGHPUT	0x0080
#define OPT_UTMP	0x0100

#define MAX_ENDDISC_CLASS 5

#define Enabled(b, o) ((b)->cfg.opt & (o))

struct sockaddr_un;
struct datalink;
struct physical;
struct link;
struct server;
struct prompt;
struct iface;

struct bundle {
  struct descriptor desc;     /* really all our datalinks */
  int unit;                   /* The device/interface unit number */
  const char **argv;          /* From main() */
  const char *argv0;          /* Original */
  const char *argv1;          /* Original */

  struct {
    char Name[20];            /* The /dev/XXXX name */
    int fd;                   /* The /dev/XXXX descriptor */
  } dev;

  u_long ifSpeed;             /* struct tuninfo speed */
  struct iface *iface;        /* Interface information */

  int routing_seq;            /* The current routing sequence number */
  u_int phase;                /* Curent phase */

  struct {
    int all;                  /* Union of all physical::type's */
    int open;                 /* Union of all open physical::type's */
  } phys_type;

  unsigned CleaningUp : 1;    /* Going to exit.... */
  unsigned AliasEnabled : 1;  /* Are we using libalias ? */

  struct fsm_parent fsm;      /* Our callback functions */
  struct datalink *links;     /* Our data links */

  struct {
    int idle_timeout;         /* NCP Idle timeout value */
    struct {
      char name[AUTHLEN];     /* PAP/CHAP system name */
      char key[AUTHLEN];      /* PAP/CHAP key */
    } auth;
    unsigned opt;             /* Uses OPT_ bits from above */
    char label[50];           /* last thing `load'ed */
    u_short mtu;              /* Interface mtu */

    struct {                  /* We need/don't need another link when  */
      struct {                /* more/less than                        */
        int packets;          /* this number of packets are queued for */
        int timeout;          /* this number of seconds                */
      } max, min;
    } autoload;

    struct {
      int timeout;            /* How long to leave the output queue choked */
    } choked;
  } cfg;

  struct {
    struct ipcp ipcp;         /* Our IPCP FSM */
    struct mp mp;             /* Our MP */
  } ncp;

  struct {
    struct filter in;         /* incoming packet filter */
    struct filter out;        /* outgoing packet filter */
    struct filter dial;       /* dial-out packet filter */
    struct filter alive;      /* keep-alive packet filter */
  } filter;

  struct {
    struct pppTimer timer;    /* timeout after cfg.idle_timeout */
    time_t done;
  } idle;

  struct {
    int fd;                   /* write status here */
  } notify;

  struct {
    struct pppTimer timer;
    time_t done;
    unsigned running : 1;
    unsigned comingup : 1;
  } autoload;

  struct {
    struct pppTimer timer;    /* choked output queue timer */
  } choked;

#ifndef NORADIUS
  struct radius radius;       /* Info retrieved from radius server */
#endif
};

#define descriptor2bundle(d) \
  ((d)->type == BUNDLE_DESCRIPTOR ? (struct bundle *)(d) : NULL)

extern struct bundle *bundle_Create(const char *, int, const char **);
extern void bundle_Destroy(struct bundle *);
extern const char *bundle_PhaseName(struct bundle *);
#define bundle_Phase(b) ((b)->phase)
extern void bundle_NewPhase(struct bundle *, u_int);
extern void bundle_LinksRemoved(struct bundle *);
extern int  bundle_LinkIsUp(const struct bundle *);
extern int bundle_SetRoute(struct bundle *, int, struct in_addr,
                           struct in_addr, struct in_addr, int, int);
extern void bundle_Close(struct bundle *, const char *, int);
extern void bundle_Down(struct bundle *, int);
extern void bundle_Open(struct bundle *, const char *, int, int);
extern void bundle_LinkClosed(struct bundle *, struct datalink *);

extern int bundle_FillQueues(struct bundle *);
extern int bundle_ShowLinks(struct cmdargs const *);
extern int bundle_ShowStatus(struct cmdargs const *);
extern void bundle_StartIdleTimer(struct bundle *);
extern void bundle_SetIdleTimer(struct bundle *, int);
extern void bundle_StopIdleTimer(struct bundle *);
extern int bundle_IsDead(struct bundle *);
extern struct datalink *bundle2datalink(struct bundle *, const char *);

extern void bundle_RegisterDescriptor(struct bundle *, struct descriptor *);
extern void bundle_UnRegisterDescriptor(struct bundle *, struct descriptor *);

extern void bundle_SetTtyCommandMode(struct bundle *, struct datalink *);

extern int bundle_DatalinkClone(struct bundle *, struct datalink *,
                                const char *);
extern void bundle_DatalinkRemove(struct bundle *, struct datalink *);
extern void bundle_CleanDatalinks(struct bundle *);
extern void bundle_SetLabel(struct bundle *, const char *);
extern const char *bundle_GetLabel(struct bundle *);
extern void bundle_SendDatalink(struct datalink *, int, struct sockaddr_un *);
extern void bundle_ReceiveDatalink(struct bundle *, int, struct sockaddr_un *);
extern int bundle_SetMode(struct bundle *, struct datalink *, int);
extern int bundle_RenameDatalink(struct bundle *, struct datalink *,
                                 const char *);
extern void bundle_setsid(struct bundle *, int);
extern void bundle_LockTun(struct bundle *);
extern int bundle_HighestState(struct bundle *);
extern int bundle_Exception(struct bundle *, int);
