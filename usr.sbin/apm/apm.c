/*
 *  Copyright (c) 1996 John T. Kohl
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <machine/apmvar.h>
#include "pathnames.h"
#include "apm-proto.h"

#define FALSE 0
#define TRUE 1

extern char *__progname;
extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

void usage(void);
void zzusage(void);
int do_zzz(const char *pn, enum apm_action action);
int open_socket(const char *pn);
int send_command(int fd,
		 struct apm_command *cmd,
		 struct apm_reply *reply);

void
usage(void)
{
    fprintf(stderr,"usage: %s [-v] [-z | -S] [-slba] [-f socket]\n",
	    __progname);
    exit(1);
}

void
zzusage(void)
{
    fprintf(stderr,"usage: %s [-z | -S] [-f socket]\n",
	    __progname);
    exit(1);
}

int
send_command(int fd,
	     struct apm_command *cmd,
	     struct apm_reply *reply)
{
    /* send a command to the apm daemon */
    cmd->vno = APMD_VNO;

    if (send(fd, cmd, sizeof(*cmd), 0) == sizeof(*cmd)) {
	if (recv(fd, reply, sizeof(*reply), 0) != sizeof(*reply)) {
	    warn("invalid reply from APM daemon\n");
	    return 1;
	}
    } else {
	warn("invalid send to APM daemon");
	return 1;
    }
    return 0;
}

int
do_zzz(const char *pn, enum apm_action action)
{
    struct apm_command command;
    struct apm_reply reply;
    int fd;

    switch (action) {
    case NONE:
    case SUSPEND:
	command.action = SUSPEND;
	break;
    case STANDBY:
	command.action = STANDBY;
	break;
    default:
	zzusage();
    }
    fd = open_socket(pn);

    if (fd == -1)
	err(1, "cannot open connection to APM daemon");
    printf("Suspending system...\n");
    exit(send_command(fd, &command, &reply));
}

int
open_socket(const char *sockname)
{
    int sock, errr;
    struct sockaddr_un s_un;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1)
	err(1, "cannot create local socket");

    s_un.sun_family = AF_UNIX;
    strncpy(s_un.sun_path, sockname, sizeof(s_un.sun_path));
    s_un.sun_len = SUN_LEN(&s_un);
    if (connect(sock, (struct sockaddr *)&s_un, s_un.sun_len) == -1) {
	errr = errno;
	close(sock);
	errno = errr;
	return -1;
    }
    return sock;
}

void
main(int argc, char *argv[])
{
    char *sockname = _PATH_APM_SOCKET;
    int ch;
    int dostatus = FALSE;
    int doac = FALSE;
    int dopct = FALSE;
    int dobstate = FALSE;
    int fd;
    int rval;
    int verbose = FALSE;
    enum apm_action action = NONE;
    struct apm_command command;
    struct apm_reply reply;

    while ((ch = getopt(argc, argv, "lbvadsSzf:")) != -1)
	switch(ch) {
	case 'v':
	    verbose = TRUE;
	    break;
	case 'f':
	    sockname = optarg;
	    break;
	case 'z':
	    if (action != NONE)
		usage();
	    action = SUSPEND;
	    break;
	case 'S':
	    if (action != NONE)
		usage();
	    action = STANDBY;
	    break;
	case 's':
	    if (action != NONE && action != GETSTATUS)
		usage();
	    dostatus = TRUE;
	    action = GETSTATUS;
	    break;
	case 'b':
	    if (action != NONE && action != GETSTATUS)
		usage();
	    dobstate = TRUE;
	    action = GETSTATUS;
	    break;
	case 'l':
	    if (action != NONE && action != GETSTATUS)
		usage();
	    dopct = TRUE;
	    action = GETSTATUS;
	    break;
	case 'a':
	    if (action != NONE && action != GETSTATUS)
		usage();
	    doac = TRUE;
	    action = GETSTATUS;
	    break;
	case '?':
	default:
	    usage();
	}

    if (!strcmp(__progname, "zzz")) {
	exit(do_zzz(sockname, action));
    }

    fd = open_socket(sockname);

    switch (action) {
    case NONE:
	verbose = doac = dopct = dobstate = dostatus = TRUE;
	action = GETSTATUS;
	/* fallthrough */
    case GETSTATUS:
	if (fd == -1) {
	    /* open the device directly and get status */
	    fd = open(_PATH_APM_NORMAL, O_RDONLY);
	    if (fd == -1) {
		err(1, "cannot contact APM daemon and cannot open " _PATH_APM_NORMAL);
	    }
	    if (ioctl(fd, APM_IOC_GETPOWER, &reply.batterystate) == 0)
		goto printval;
	}
    case SUSPEND:
    case STANDBY:
	command.action = action;
	break;
    default:
	usage();
    }
    
    if ((rval = send_command(fd, &command, &reply)) == 0) {
	switch (action) {
	case GETSTATUS:
    printval:
	    if (verbose) {
		if (dobstate)
		    printf("Battery charge state: %s\n",
			   battstate(reply.batterystate.battery_state));
		if (dopct)
		    printf("Battery remaining: %d percent\n",
			   reply.batterystate.battery_life);
		if (doac)
		    printf("A/C adapter state: %s\n", ac_state(reply.batterystate.ac_state));
		if (dostatus)
		    printf("Power management enabled\n");
	    } else {
		if (dobstate)
		    printf("%d\n", reply.batterystate.battery_state);
		if (dopct)
		    printf("%d\n", reply.batterystate.battery_life);
		if (doac)
		    printf("%d\n", reply.batterystate.ac_state);
		if (dostatus)
		    printf("1\n");
	    }
	    break;
	default:
	    break;
	}
	switch (reply.newstate) {
	case SUSPEND:
	    printf("System will enter suspend mode momentarily.\n");
	    break;
	case STANDBY:
	    printf("System will enter standby mode momentarily.\n");
	    break;
	default:
	    break;
	}
    } else
	errx(rval, "cannot get reply from APM daemon\n");

    exit(0);
}
