/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      H�gskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "appl_locl.h"
#include <sl.h>
#include "bos_local.h"
#include <bos.h>
#include <bos.cs.h>
/* #include <time.h> we schould .... */

RCSID("$Id: bos_getrestart.c,v 1.2 2000/08/28 12:00:59 assar Exp $");

static int
printrestart(const char *cell, const char *host,
	    int noauth, int localauth, int verbose)
{
  struct rx_connection *connvolser = NULL;
  struct bozo_netKTime time;
  int error;
  
  connvolser = arlalib_getconnbyname(cell,
				     host,
				     afsbosport,
				     BOS_SERVICE_ID,
				     arlalib_getauthflag(noauth,
							 localauth,0,0));

  if (connvolser == NULL)
    return -1;

  if (( error = BOZO_GetRestartTime(connvolser, 1, &time)) == 0) {
    printf ("Server %s restarts at %02d:%02d:%02d at day %d\n", host, time.hour, time.min, time.sec, time.day);

  }
  if (( error = BOZO_GetRestartTime(connvolser, 2, &time)) == 0) {
    printf ("Server %s restarts for new binaries at %02d:%02d:%02d at day %d\n", host, time.hour, time.min, time.sec, time.day);
  }
  
  arlalib_destroyconn(connvolser);
  return 0;
}


static int helpflag;
static const char *server;
static const char *cell;
static int noauth;
static int localauth;
static int verbose;

static struct getargs args[] = {
  {"server",	0, arg_string,	&server,	"server", NULL, arg_mandatory},
  {"cell",	0, arg_string,	&cell,		"cell",	  NULL},
  {"noauth",	0, arg_flag,	&noauth,	"do not authenticate", NULL},
  {"local",	0, arg_flag,	&localauth,	"localauth"},
  {"verbose",	0, arg_flag,	&verbose,	"be verbose", NULL},
  {"help",	0, arg_flag,	&helpflag,	NULL, NULL},
  {NULL,	0, arg_end,	NULL,		NULL, NULL}
};

static void
usage (void)
{
  arg_printusage (args, "bos getrestart", "", ARG_AFSSTYLE);
}

int
bos_getrestart(int argc, char **argv)
{
  int optind = 0;

  if (getarg (args, argc, argv, &optind, ARG_AFSSTYLE)) {
    usage ();
    return 0;
  }

  if (helpflag) {
    usage ();
    return 0;
  }

  argc -= optind;
  argv += optind;

  if (server == NULL) {
    printf ("bos getrestart: missing -server\n");
    return 0;
  }

  if (cell == NULL)
    cell = cell_getcellbyhost (server);

  printrestart (cell, server, noauth, localauth, verbose);
  return 0;
}
