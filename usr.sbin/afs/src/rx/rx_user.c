/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*
 * rx_user.c contains routines specific to the user space UNIX
 * implementation of rx
 */

#include "rx_locl.h"

RCSID("$arla: rx_user.c,v 1.21 2003/04/08 22:14:04 lha Exp $");

#ifndef	IPPORT_USERRESERVED
/*
 * If in.h doesn't define this, define it anyway.  Unfortunately, defining
 * this doesn't put the code into the kernel to restrict kernel assigned
 * port numbers to numbers below IPPORT_USERRESERVED...
 */
#define IPPORT_USERRESERVED 5000
#endif

static osi_socket *rx_sockets = NULL;
static int num_rx_sockets = 0;

static fd_set rx_selectMask;
static int rx_maxSocketNumber = -1;    /* Maximum socket number represented
				        * in the select mask */
static PROCESS rx_listenerPid;	       /* LWP process id of socket listener
				        * process */
void rxi_Listener(void);

/*
 * This routine will get called by the event package whenever a new,
 * earlier than others, event is posted.  If the Listener process
 * is blocked in selects, this will unblock it.  It also can be called
 * to force a new trip through the rxi_Listener select loop when the set
 * of file descriptors it should be listening to changes...
 */
void
rxi_ReScheduleEvents(void)
{
    if (rx_listenerPid)
	IOMGR_Cancel(rx_listenerPid);
}

void
rxi_StartListener(void)
{
    /* Initialize LWP & IOMGR in case no one else has */
    PROCESS junk;

    LWP_InitializeProcessSupport(LWP_NORMAL_PRIORITY, &junk);
    IOMGR_Initialize();

    /* Priority of listener should be high, so it can keep conns alive */
#define	RX_LIST_STACK	24000
    LWP_CreateProcess(rxi_Listener, RX_LIST_STACK, LWP_MAX_PRIORITY, 0,
		      "rx_Listener", &rx_listenerPid);
}

/*
 * Called by rx_StartServer to start up lwp's to service calls.
 * NExistingProcs gives the number of procs already existing, and which
 * therefore needn't be created.
 */
void
rxi_StartServerProcs(int nExistingProcs)
{
    struct rx_service *service;
    int i;
    int maxdiff = 0;
    int nProcs = 0;
    PROCESS scratchPid;

    /*
     * For each service, reserve N processes, where N is the "minimum" number
     * of processes that MUST be able to execute a request in parallel, at
     * any time, for that process.  Also compute the maximum difference
     * between any service's maximum number of processes that can run (i.e.
     * the maximum number that ever will be run, and a guarantee that this
     * number will run if other services aren't running), and its minimum
     * number.  The result is the extra number of processes that we need in
     * order to provide the latter guarantee
     */
    for (i = 0; i < RX_MAX_SERVICES; i++) {
	int diff;

	service = rx_services[i];
	if (service == (struct rx_service *) 0)
	    break;
	nProcs += service->minProcs;
	diff = service->maxProcs - service->minProcs;
	if (diff > maxdiff)
	    maxdiff = diff;
    }
    nProcs += maxdiff;		       /* Extra processes needed to allow max
				        * number requested to run in any
				        * given service, under good
				        * conditions */
    nProcs -= nExistingProcs;	       /* Subtract the number of procs that
				        * were previously created for use as
				        * server procs */
    for (i = 0; i < nProcs; i++) {
	LWP_CreateProcess(rx_ServerProc, rx_stackSize, RX_PROCESS_PRIORITY, 0,
			  "rx_ServerProc", &scratchPid);
    }
}

/*
 * Make a socket for receiving/sending IP packets.  Set it into non-blocking
 * and large buffering modes.  If port isn't specified, the kernel will pick
 * one.  Returns the socket (>= 0) on success.  Returns OSI_NULLSOCKET on
 * failure.
 *
 * Port must be in network byte order.
 */

osi_socket
rxi_GetUDPSocket(uint16_t port, uint16_t *retport)
{
    int code;
    osi_socket socketFd = OSI_NULLSOCKET;
    osi_socket *sockets;
    struct sockaddr_in taddr;
    char *name = "rxi_GetUDPSocket: ";
    int sa_size;

    sockets = realloc(rx_sockets, (num_rx_sockets + 1) * sizeof(*rx_sockets));
    if (sockets == NULL) {
	perror("socket");
	osi_Msg(("%sunable to allocated memory for UDP socket\n", name));
	return OSI_NULLSOCKET;
    }
    rx_sockets = sockets;

    socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd < 0) {
	perror("socket");
	osi_Msg(("%sunable to create UDP socket\n", name));
	return OSI_NULLSOCKET;
    }

    if (socketFd >= FD_SETSIZE) {
	osi_Msg(("socket fd too large\n"));
	close(socketFd);
	return OSI_NULLSOCKET;
    }

    rx_sockets[num_rx_sockets] = socketFd;
    num_rx_sockets++;

#ifdef SO_BSDCOMPAT
    {
	int one = 1;
	setsockopt (socketFd, SOL_SOCKET, SO_BSDCOMPAT, &one, sizeof(one));
    }
#endif    

    if (rx_maxSocketNumber < 0)
	FD_ZERO(&rx_selectMask);

    FD_SET(socketFd, &rx_selectMask);
    if (socketFd > rx_maxSocketNumber)
	rx_maxSocketNumber = socketFd;

    memset (&taddr, 0, sizeof(taddr));
    taddr.sin_family = AF_INET;
    taddr.sin_port   = port;

    code = bind(socketFd, (struct sockaddr *) &taddr, sizeof(taddr));
    if (code < 0) {
	perror("bind");
	osi_Msg(("%sunable to bind UDP socket\n", name));
	goto error;
    }

    sa_size = sizeof(taddr);
    code = getsockname(socketFd, (struct sockaddr *) &taddr, &sa_size);
    if (code < 0) {
	perror("getsockname");
	osi_Msg(("%sunable to bind UDP socket\n", name));
	goto error;
    }
    if (retport)
	*retport = taddr.sin_port;

    /*
     * Use one of three different ways of getting a socket buffer expanded to
     * a reasonable size
     */
    {
	int len1, len2;

	len1 = len2 = 32766;

	rx_stats.socketGreedy =
	    (setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF,
			&len1, sizeof(len1)) >= 0) &&
	    (setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF,
			&len2, sizeof(len2)) >= 0);
    }

    if (!rx_stats.socketGreedy)
	osi_Msg(("%s*WARNING* Unable to increase buffering on socket\n",name));

    /*
     * Put it into non-blocking mode so that rx_Listener can do a polling
     * read before entering select
     */
    if (fcntl(socketFd, F_SETFL, FNDELAY) == -1) {
	perror("fcntl");
	osi_Msg(("%sunable to set non-blocking mode on socket\n", name));
	goto error;
    }
    return socketFd;

error:
    num_rx_sockets--;
    rx_sockets[num_rx_sockets] = OSI_NULLSOCKET;

    close(socketFd);

    return OSI_NULLSOCKET;
}

/*
 * The main loop which listens to the net for datagrams, and handles timeouts
 * and retransmissions, etc.  It also is responsible for scheduling the
 * execution of pending events (in conjunction with event.c).
 *
 * Note interaction of nextPollTime and lastPollWorked.  The idea is
 * that if rx is not keeping up with the incoming stream of packets
 * (because there are threads that are interfering with its running
 * sufficiently often), rx does a polling select using IOMGR_Select
 * (setting tv_sec = tv_usec = 0). Old code is a system select, but
 * this was bad since we didn't know what calling conversion the
 * system select() was using (on win32 hosts it was PASCAL, and you
 * lost your $sp)
 *
 * So, our algorithm is that if the last poll on the file descriptor found
 * useful data, or we're at the time nextPollTime (which is advanced so that
 * it occurs every 3 or 4 seconds),
 * then we try the polling select.  If we eventually
 * catch up (which we can tell by the polling select returning no input
 * packets ready), then we don't do a polling select again until several
 * seconds later (via nextPollTime mechanism).
 */

#ifndef FD_COPY
#define FD_COPY(f, t)	memcpy((t), (f), sizeof(*(f)))
#endif

void
rxi_Listener(void)
{
    uint32_t host;
    uint16_t port;
    struct rx_packet *p = NULL;
    fd_set rfds;
    int socket;
    int fds;
    struct clock cv;
    long nextPollTime;		       /* time to next poll FD before
				        * sleeping */
    int lastPollWorked, doingPoll;     /* true iff last poll was useful */
    struct timeval tv, *tvp;

    clock_NewTime();
    lastPollWorked = 0;
    nextPollTime = 0;
    for (;;) {

	/*
	 * Grab a new packet only if necessary (otherwise re-use the old one)
	 */
	if (p == NULL) {
	    if ((p = rxi_AllocPacket(RX_PACKET_CLASS_RECEIVE)) == NULL)
		osi_Panic("rxi_Listener: no packets!");	/* Shouldn't happen */
	}
	/* Wait for the next event time or a packet to arrive. */
	/*
	 * event_RaiseEvents schedules any events whose time has come and
	 * then atomically computes the time to the next event, guaranteeing
	 * that this is positive.  If there is no next event, it returns 0
	 */
	if (!rxevent_RaiseEvents(&cv))
	    tvp = NULL;
	else {

	    /*
	     * It's important to copy cv to tv, because the 4.3 documentation
	     * for select threatens that *tv may be updated after a select,
	     * in future editions of the system, to indicate how much of the
	     * time period has elapsed.  So we shouldn't rely on tv not being
	     * altered.
	     */
	    tv.tv_sec = cv.sec;	       /* Time to next event */
	    tv.tv_usec = cv.usec;
	    tvp = &tv;
	}
	rx_stats.selects++;
	FD_COPY(&rx_selectMask, &rfds);
	if (lastPollWorked || nextPollTime < clock_Sec()) {
	    /* we're catching up, or haven't tried to for a few seconds */
	    doingPoll = 1;
	    nextPollTime = clock_Sec() + 4;	/* try again in 4 seconds no
						 * matter what */
	    tv.tv_sec = tv.tv_usec = 0;/* make sure we poll */
	    tvp = &tv;
	} else {
	    doingPoll = 0;
	}
	lastPollWorked = 0;	       /* default is that it didn't find
				        * anything */

	fds = IOMGR_Select (rx_maxSocketNumber + 1, &rfds, 0, 0, tvp);
	clock_NewTime();
	if (fds > 0) {
	    if (doingPoll)
		lastPollWorked = 1;

	    for (socket = 0; socket < num_rx_sockets; socket++) {
		if (p == NULL)
		    break;
		if (FD_ISSET(rx_sockets[socket], &rfds) &&
		    rxi_ReadPacket(rx_sockets[socket], p, &host, &port))
		{
		    p = rxi_ReceivePacket(p, rx_sockets[socket], host, port);
		}
	    }
	}
    }
    /* NOTREACHED */
}


void
osi_Panic(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Fatal Rx error: ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
    fflush(stdout);
    abort();
}

void
osi_vMsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}


#define	ADDRSPERSITE	256

#ifdef ADAPT_MTU

static u_long myNetAddrs[ADDRSPERSITE];
static int myNetMTUs[ADDRSPERSITE];
static int myNetFlags[ADDRSPERSITE];
static int numMyNetAddrs;

static void
GetIFInfo(void)
{
    int s;
    int len, res;
    struct ifconf ifc;
    struct ifreq ifs[ADDRSPERSITE];
    struct sockaddr_in *a;
    char *p;
    struct ifreq ifreq;
    size_t sz = 0;

    numMyNetAddrs = 0;
    memset(myNetAddrs, 0,  sizeof(myNetAddrs));
    memset(myNetMTUs, 0,  sizeof(myNetMTUs));
    memset(myNetFlags, 0,  sizeof(myNetFlags));

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	return;

    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_buf = (caddr_t) & ifs[0];
    memset(&ifs[0], 0,  sizeof(ifs));

    res = ioctl(s, SIOCGIFCONF, &ifc);
    if (res < 0) {
	close(s);
	return;
    }
    len = ifc.ifc_len / sizeof(struct ifreq);
    if (len > ADDRSPERSITE)
	len = ADDRSPERSITE;

    ifreq.ifr_name[0] = '\0';
    for (p = ifc.ifc_buf; p < ifc.ifc_buf + ifc.ifc_len; p += sz) {
	struct ifreq *ifr = (struct ifreq *)p;

	sz = sizeof(*ifr);
#ifdef SOCKADDR_HAS_SA_LEN
	sz = max(sz, sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len);
#endif
	  if (strncmp (ifreq.ifr_name,
		       ifr->ifr_name,
		       sizeof(ifr->ifr_name))) {
	      res = ioctl(s, SIOCGIFFLAGS, ifr);
	      if (res < 0)
		  continue;
	      if (!(ifr->ifr_flags & IFF_UP))
		  continue;
	      if (ifr->ifr_flags & IFF_LOOPBACK)
		  continue;
	      myNetFlags[numMyNetAddrs] = ifr->ifr_flags;

	      res = ioctl(s, SIOCGIFADDR, ifr);
	      if (res < 0)
		  continue;
	      a = (struct sockaddr_in *)&ifr->ifr_addr;
	      if (a->sin_family != AF_INET)
		  continue;
	      myNetAddrs[numMyNetAddrs] = ntohl(a->sin_addr.s_addr);

	      res = -1;
#ifdef SIOCGIFMTU
	      res = ioctl(s, SIOCGIFMTU, ifr);
#elif SIOCRIFMTU
	      res = ioctl(s, SIOCRIFMTU, ifr);
#else
	      res = -1;
#endif
	      if (res == 0) {
		  myNetMTUs[numMyNetAddrs] = ifr->ifr_metric;
		  if (rx_maxReceiveSize < (myNetMTUs[numMyNetAddrs]
					   - RX_IPUDP_SIZE))
		      rx_maxReceiveSize = MIN(RX_MAX_PACKET_SIZE,
					      (myNetMTUs[numMyNetAddrs]
					       - RX_IPUDP_SIZE));

		  if (rx_MyMaxSendSize < myNetMTUs[numMyNetAddrs]
					   - RX_IPUDP_SIZE)
		      rx_MyMaxSendSize = myNetMTUs[numMyNetAddrs]
			  - RX_IPUDP_SIZE;
	      
	      } else {
		  myNetMTUs[numMyNetAddrs] = OLD_MAX_PACKET_SIZE;
		  res = 0;
	      }
	      ++numMyNetAddrs;
	      ifreq = *ifr;
	  }
    }

	{
#if 0
	    RETSIGTYPE (*old)(int);

	    old = signal(SIGSYS, SIG_IGN);
	    if (syscall(31 /* AFS_SYSCALL */ , 28 /* AFSCALL_CALL */ ,
			20 /* AFSOP_GETMTU */ , myNetAddrs[numMyNetAddrs],
			&(myNetMTUs[numMyNetAddrs])));
	    myNetMTUs[numMyNetAddrs] = OLD_MAX_PACKET_SIZE;
	    signal(SIGSYS, old);
#endif
	}

    close(s);

    /*
     * have to allocate at least enough to allow a single packet to reach its
     * maximum size, so ReadPacket will work.  Allocate enough for a couple
     * of packets to do so, for good measure
     */
    /* MTUXXX before shipping, change this 8 to a 4 */
    {
	int npackets, ncbufs;

	ncbufs = (rx_maxReceiveSize - RX_FIRSTBUFFERSIZE);
	if (ncbufs > 0) {
	    ncbufs = ncbufs / RX_CBUFFERSIZE;
	    npackets = (rx_Window / 8);
	    npackets = (npackets > 2 ? npackets : 2);
	    rxi_MoreCbufs(npackets * (ncbufs + 1));
	}
    }
}

#endif				       /* ADAPT_MTU */

/*
 * Called from rxi_FindPeer, when initializing a clear rx_peer structure,
 * to get interesting information.
 */

void
rxi_InitPeerParams(struct rx_peer * pp)
{
    uint32_t ppaddr, msk, net;
    int rxmtu;
    int ix, nlix = 0, nlcount;
    static int Inited = 0;

#ifdef ADAPT_MTU

    if (!Inited) {
	GetIFInfo();
	Inited = 1;
    }
    /*
     * try to second-guess IP, and identify which link is most likely to
     * be used for traffic to/from this host.
     */
    ppaddr = ntohl(pp->host);
    if (IN_CLASSA(ppaddr))
	msk = IN_CLASSA_NET;
    else if (IN_CLASSB(ppaddr))
	msk = IN_CLASSB_NET;
    else if (IN_CLASSC(ppaddr))
	msk = IN_CLASSC_NET;
    else
	msk = 0;
    net = ppaddr & msk;

    for (nlcount = 0, ix = 0; ix < numMyNetAddrs; ++ix) {
#ifdef IFF_LOOPBACK
	if (!(myNetFlags[ix] & IFF_LOOPBACK)) {
	    nlix = ix;
	    ++nlcount;
	}
#endif				       /* IFF_LOOPBACK */
	if ((myNetAddrs[ix] & msk) == net)
	    break;
    }

    pp->rateFlag = 2;		       /* start timing after two full packets */
    /*
     * I don't initialize these, because I presume they are bzero'd...
     * pp->burstSize pp->burst pp->burstWait.sec pp->burstWait.usec
     * pp->timeout.usec
     */

    pp->maxWindow = rx_Window;
    if (ix >= numMyNetAddrs) {	       /* not local */
	pp->timeout.sec = 3;
	pp->packetSize = RX_REMOTE_PACKET_SIZE;
    } else {
	pp->timeout.sec = 2;
	pp->packetSize = MIN(RX_MAX_PACKET_SIZE,
			     (rx_MyMaxSendSize + RX_HEADER_SIZE));
    }

    /* Now, maybe get routing interface and override parameters. */
    if (ix >= numMyNetAddrs && nlcount == 1)
	ix = nlix;

    if (ix < numMyNetAddrs) {
#ifdef IFF_POINTOPOINT
	if (myNetFlags[ix] & IFF_POINTOPOINT) {
	    /* wish we knew the bit rate and the chunk size, sigh. */
	    pp->maxWindow = 10;
	    pp->timeout.sec = 4;
	    /* pp->timeout.usec = 0; */
	    pp->packetSize = RX_PP_PACKET_SIZE;
	}
#endif				       /* IFF_POINTOPOINT */

	/*
	 * Reduce the packet size to one based on the MTU given by the
	 * interface.
	 */
	if (myNetMTUs[ix] > (RX_IPUDP_SIZE + RX_HEADER_SIZE)) {
	    rxmtu = myNetMTUs[ix] - RX_IPUDP_SIZE;
	    if (rxmtu < pp->packetSize)
		pp->packetSize = rxmtu;
	}
    }
#else				       /* ADAPT_MTU */
    pp->rateFlag = 2;		       /* start timing after two full packets */
    pp->maxWindow = rx_Window;
    pp->timeout.sec = 2;
    pp->packetSize = OLD_MAX_PACKET_SIZE;
#endif				       /* ADAPT_MTU */
}
