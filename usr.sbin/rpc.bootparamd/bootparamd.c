/*
 * This code is not copyright, and is placed in the public domain.
 * Feel free to use and modify. Please send modifications and/or
 * suggestions + bug fixes to Klas Heggemann <klas@nada.kth.se>
 *
 * Various small changes by Theo de Raadt <deraadt@fsa.ca>
 * Parser rewritten (adding YP support) by Roland McGrath <roland@frob.com>
 *
 * $Id: bootparamd.c,v 1.5 1995/06/24 15:03:53 pk Exp $
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpcsvc/bootparam_prot.h>
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include "pathnames.h"

#define MAXLEN 800

struct hostent *he;
static char buffer[MAXLEN];
static char hostname[MAX_MACHINE_NAME];
static char askname[MAX_MACHINE_NAME];
static char path[MAX_PATH_LEN];
static char domain_name[MAX_MACHINE_NAME];

extern void bootparamprog_1 __P((struct svc_req *, SVCXPRT *));

int	_rpcsvcdirty = 0;
int	_rpcpmstart = 0;
int     debug = 0;
int     dolog = 0;
unsigned long route_addr, inet_addr();
struct sockaddr_in my_addr;
char   *progname;
char   *bootpfile = _PATH_BOOTPARAMS;

extern char *optarg;
extern int optind;

void
usage()
{
	fprintf(stderr,
	    "usage: rpc.bootparamd [-d] [-s] [-r router] [-f bootparmsfile]\n");
}


/*
 * ever familiar
 */
int
main(argc, argv)
	int     argc;
	char  **argv;
{
	SVCXPRT *transp;
	int     i, s, pid;
	char   *rindex();
	struct hostent *he;
	struct stat buf;
	char   *optstring;
	char    c;

	progname = rindex(argv[0], '/');
	if (progname)
		progname++;
	else
		progname = argv[0];

	while ((c = getopt(argc, argv, "dsr:f:")) != EOF)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'r':
			if (isdigit(*optarg)) {
				route_addr = inet_addr(optarg);
				break;
			}
			he = gethostbyname(optarg);
			if (!he) {
				fprintf(stderr, "%s: No such host %s\n",
				    progname, optarg);
				usage();
				exit(1);
			}
			bcopy(he->h_addr, (char *) &route_addr, sizeof(route_addr));
			break;
		case 'f':
			bootpfile = optarg;
			break;
		case 's':
			dolog = 1;
#ifndef LOG_DAEMON
			openlog(progname, 0, 0);
#else
			openlog(progname, 0, LOG_DAEMON);
			setlogmask(LOG_UPTO(LOG_NOTICE));
#endif
			break;
		default:
			usage();
			exit(1);
		}

	if (stat(bootpfile, &buf)) {
		fprintf(stderr, "%s: ", progname);
		perror(bootpfile);
		exit(1);
	}
	if (!route_addr) {
		get_myaddress(&my_addr);
		bcopy(&my_addr.sin_addr.s_addr, &route_addr, sizeof(route_addr));
	}
	if (!debug)
		daemon();

	(void) pmap_unset(BOOTPARAMPROG, BOOTPARAMVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf(stderr, "cannot create udp service.\n");
		exit(1);
	}
	if (!svc_register(transp, BOOTPARAMPROG, BOOTPARAMVERS,
		bootparamprog_1, IPPROTO_UDP)) {
		fprintf(stderr,
		    "bootparamd: unable to register BOOTPARAMPROG version %d, udp)\n",
		    BOOTPARAMVERS);
		exit(1);
	}
	svc_run();
	fprintf(stderr, "svc_run returned\n");
	exit(1);
}

bp_whoami_res *
bootparamproc_whoami_1_svc(whoami, rqstp)
	bp_whoami_arg *whoami;
	struct svc_req *rqstp;
{
	long    haddr;
	static bp_whoami_res res;

	if (debug)
		fprintf(stderr, "whoami got question for %d.%d.%d.%d\n",
		    255 & whoami->client_address.bp_address_u.ip_addr.net,
		    255 & whoami->client_address.bp_address_u.ip_addr.host,
		    255 & whoami->client_address.bp_address_u.ip_addr.lh,
		    255 & whoami->client_address.bp_address_u.ip_addr.impno);
	if (dolog)
		syslog(LOG_NOTICE, "whoami got question for %d.%d.%d.%d\n",
		    255 & whoami->client_address.bp_address_u.ip_addr.net,
		    255 & whoami->client_address.bp_address_u.ip_addr.host,
		    255 & whoami->client_address.bp_address_u.ip_addr.lh,
		    255 & whoami->client_address.bp_address_u.ip_addr.impno);

	bcopy((char *) &whoami->client_address.bp_address_u.ip_addr, (char *) &haddr,
	    sizeof(haddr));
	he = gethostbyaddr((char *) &haddr, sizeof(haddr), AF_INET);
	if (!he)
		goto failed;

	if (debug)
		fprintf(stderr, "This is host %s\n", he->h_name);
	if (dolog)
		syslog(LOG_NOTICE, "This is host %s\n", he->h_name);

	strcpy(askname, he->h_name);
	if (!lookup_bootparam(askname, hostname, NULL, NULL, NULL)) {
		res.client_name = hostname;
		getdomainname(domain_name, MAX_MACHINE_NAME);
		res.domain_name = domain_name;

		if (res.router_address.address_type != IP_ADDR_TYPE) {
			res.router_address.address_type = IP_ADDR_TYPE;
			bcopy(&route_addr, &res.router_address.bp_address_u.ip_addr, 4);
		}
		if (debug)
			fprintf(stderr, "Returning %s   %s    %d.%d.%d.%d\n",
			    res.client_name, res.domain_name,
			    255 & res.router_address.bp_address_u.ip_addr.net,
			    255 & res.router_address.bp_address_u.ip_addr.host,
			    255 & res.router_address.bp_address_u.ip_addr.lh,
			    255 & res.router_address.bp_address_u.ip_addr.impno);
		if (dolog)
			syslog(LOG_NOTICE, "Returning %s   %s    %d.%d.%d.%d\n",
			    res.client_name, res.domain_name,
			    255 & res.router_address.bp_address_u.ip_addr.net,
			    255 & res.router_address.bp_address_u.ip_addr.host,
			    255 & res.router_address.bp_address_u.ip_addr.lh,
			    255 & res.router_address.bp_address_u.ip_addr.impno);

		return (&res);
	}
failed:
	if (debug)
		fprintf(stderr, "whoami failed\n");
	if (dolog)
		syslog(LOG_NOTICE, "whoami failed\n");
	return (NULL);
}


bp_getfile_res *
bootparamproc_getfile_1_svc(getfile, rqstp)
	bp_getfile_arg *getfile;
	struct svc_req *rqstp;
{
	char   *where, *index();
	static bp_getfile_res res;
	int     err;

	if (debug)
		fprintf(stderr, "getfile got question for \"%s\" and file \"%s\"\n",
		    getfile->client_name, getfile->file_id);

	if (dolog)
		syslog(LOG_NOTICE, "getfile got question for \"%s\" and file \"%s\"\n",
		    getfile->client_name, getfile->file_id);

	he = NULL;
	he = gethostbyname(getfile->client_name);
	if (!he)
		goto failed;

	strcpy(askname, he->h_name);
	err = lookup_bootparam(askname, NULL, getfile->file_id,
	    &res.server_name, &res.server_path);
	if (err == 0) {
		he = gethostbyname(res.server_name);
		if (!he)
			goto failed;
		bcopy(he->h_addr, &res.server_address.bp_address_u.ip_addr, 4);
		res.server_address.address_type = IP_ADDR_TYPE;
	} else if (err == ENOENT && !strcmp(getfile->file_id, "dump")) {
		/* Special for dump, answer with null strings. */
		res.server_name[0] = '\0';
		res.server_path[0] = '\0';
		bzero(&res.server_address.bp_address_u.ip_addr, 4);
	} else {
failed:
		if (debug)
			fprintf(stderr, "getfile failed for %s\n",
			    getfile->client_name);
		if (dolog)
			syslog(LOG_NOTICE,
			    "getfile failed for %s\n", getfile->client_name);
		return (NULL);
	}

	if (debug)
		fprintf(stderr,
		    "returning server:%s path:%s address: %d.%d.%d.%d\n",
		    res.server_name, res.server_path,
		    255 & res.server_address.bp_address_u.ip_addr.net,
		    255 & res.server_address.bp_address_u.ip_addr.host,
		    255 & res.server_address.bp_address_u.ip_addr.lh,
		    255 & res.server_address.bp_address_u.ip_addr.impno);
	if (dolog)
		syslog(LOG_NOTICE,
		    "returning server:%s path:%s address: %d.%d.%d.%d\n",
		    res.server_name, res.server_path,
		    255 & res.server_address.bp_address_u.ip_addr.net,
		    255 & res.server_address.bp_address_u.ip_addr.host,
		    255 & res.server_address.bp_address_u.ip_addr.lh,
		    255 & res.server_address.bp_address_u.ip_addr.impno);
	return (&res);
}


int
lookup_bootparam(client, client_canonical, id, server, path)
	char	*client;
	char	*client_canonical;
	char	*id;
	char	**server;
	char	**path;
{
	FILE   *f = fopen(bootpfile, "r");
#ifdef YP
	static char *ypbuf = NULL;
	static int ypbuflen = 0;
#endif
	static char buf[BUFSIZ];
	char   *bp, *word;
	size_t  idlen = id == NULL ? 0 : strlen(id);
	int     contin = 0;
	int     found = 0;

	if (f == NULL)
		return EINVAL;	/* ? */

	while (fgets(buf, sizeof buf, f)) {
		int     wascontin = contin;
		contin = buf[strlen(buf) - 2] == '\\';
		bp = buf + strspn(buf, " \t\n");

		switch (wascontin) {
		case -1:
			/* Continuation of uninteresting line */
			contin *= -1;
			continue;
		case 0:
			/* New line */
			contin *= -1;
			if (*bp == '#')
				continue;
			if ((word = strsep(&bp, " \t\n")) == NULL)
				continue;
#ifdef YP
			/* A + in the file means try YP now */
			if (!strcmp(word, "+")) {
				char   *ypdom;

				if (yp_get_default_domain(&ypdom) ||
				    yp_match(ypdom, "bootparams", client,
					strlen(client), &ypbuf, &ypbuflen))
					continue;
				bp = ypbuf;
				word = client;
				contin *= -1;
				break;
			}
#endif
			/* See if this line's client is the one we are
			 * looking for */
			if (strcmp(word, client) != 0) {
				/*
				 * If it didn't match, try getting the
				 * canonical host name of the client
				 * on this line and comparing that to
				 * the client we are looking for
				 */
				struct hostent *hp = gethostbyname(word);
				if (hp == NULL || strcmp(hp->h_name, client))
					continue;
			}
			contin *= -1;
			break;
		case 1:
			/* Continued line we want to parse below */
			break;
		}

		if (client_canonical)
			strncpy(client_canonical, word, MAX_MACHINE_NAME);

		/* We have found a line for CLIENT */
		if (id == NULL) {
			(void) fclose(f);
			return 0;
		}

		/* Look for a value for the parameter named by ID */
		while ((word = strsep(&bp, " \t\n")) != NULL) {
			if (!strncmp(word, id, idlen) && word[idlen] == '=') {
				/* We have found the entry we want */
				*server = &word[idlen + 1];
				*path = strchr(*server, ':');
				if (*path == NULL)
					/* Malformed entry */
					continue;
				*(*path)++ = '\0';
				(void) fclose(f);
				return 0;
			}
		}

		found = 1;
	}

	(void) fclose(f);
	return found ? ENOENT : EPERM;
}
