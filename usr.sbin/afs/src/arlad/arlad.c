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
 * 3. Neither the name of the Institute nor the names of its contributors
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

#include <arla_local.h>
#include <getarg.h>
#include <arla-version.h>
RCSID("$KTH: arlad.c,v 1.5.2.2 2001/07/26 02:05:26 assar Exp $");

#define KERNEL_STACKSIZE (16*1024)

char *default_log_file = "syslog";
int client_port = 4711;

static char *pid_filename;


/*
 *
 */


static void
write_pid_file (const char *progname)
{
    FILE *fp;

    asprintf (&pid_filename, "/var/run/%s.pid", progname);
    if (pid_filename == NULL)
	return;
    fp = fopen (pid_filename, "w");
    if (fp == NULL)
	return;
    fprintf (fp, "%u", (unsigned)getpid());
    fclose (fp);
}

static void
delete_pid_file (void)
{
    if (pid_filename != NULL) {
	unlink (pid_filename);
	free (pid_filename);
	pid_filename = NULL;
    }
}

/*
 * signal handlers...
 */

static void
sigint (int foo)
{
    arla_warnx (ADEBMISC, "fatal signal received");
    store_state ();
    delete_pid_file ();
    exit (0);
}

static void
sighup (int foo)
{
    store_state ();
    delete_pid_file ();
}

static void
sigusr1 (int foo)
{
    exit(0);
}

static void
sigchild (int foo)
{
    exit(1);
}

static void
daemonify (void)
{
    pid_t pid;
    int fd;

    pid = fork ();
    if (pid < 0)
	arla_err (1, ADEBERROR, errno, "fork");
    else if (pid > 0) {
	signal(SIGUSR1, sigusr1);
	signal(SIGCHLD, sigchild);
	while (1) pause();
	exit(0);
    }
    if (setsid() == -1)
	arla_err (1, ADEBERROR, errno, "setsid");
    fd = open(_PATH_DEVNULL, O_RDWR, 0);
    if (fd < 0)
	arla_err (1, ADEBERROR, errno, "open " _PATH_DEVNULL);
    dup2 (fd, STDIN_FILENO);
    dup2 (fd, STDOUT_FILENO);
    dup2 (fd, STDERR_FILENO);
    if (fd > 2)
	    close (fd);
}


void
arla_start (char *device_file, const char *cache_dir)
{
    struct kernel_args kernel_args;
    PROCESS kernelpid;
    
    signal (SIGINT, sigint);
    signal (SIGTERM, sigint);
    signal (SIGHUP, sighup);
    umask (S_IRWXG|S_IRWXO); /* 077 */

#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_DUMPABLE)
    prctl(PR_SET_DUMPABLE, 1);
#endif

    xfs_message_init ();
    kernel_opendevice (device_file);
    
    kernel_args.num_workers = num_workers;
    
    if (LWP_CreateProcess (kernel_interface, KERNEL_STACKSIZE, 1,
			       (char *)&kernel_args,
			   "Kernel-interface", &kernelpid))
	arla_errx (1, ADEBERROR,
		   "Cannot create kernel-interface process");
    
    write_pid_file ("arlad");
    
    if (chroot (cache_dir) < 0)
	arla_err (1, ADEBERROR, errno, "chroot %s", cache_dir);

    if (fork_flag)
	kill(getppid(), SIGUSR1);
    
    LWP_WaitProcess ((char *)arla_start);
    abort ();
}

char *
get_default_cache_dir (void)
{
    return ARLACACHEDIR;
}

char *device_file = "/dev/xfs0";

static struct getargs args[] = {
    {"conffile", 'c',	arg_string,	&conf_file,
     "path to configuration file", "file"},
    {"check-consistency", 'C', arg_flag, &cm_consistency,
     "if we want extra paranoid consistency checks", NULL },
    {"log",	'l',	arg_string,	&log_file,
     "where to write log (stderr (default), syslog, or path to file)", NULL},
    {"debug",	0,	arg_string,	&debug_levels,
     "what to write in the log", NULL},
    {"device",	'd',	arg_string,	&device_file,
     "the XFS device to use [/dev/xfs0]", "path"},
    {"connected-mode", 0, arg_string,	&connected_mode_string,
     "initial connected mode [conncted|fetch-only|disconnected]", NULL},
    {"dynroot", 'D', arg_flag,	&dynroot_enable,
     "if dynroot is enabled", NULL},
    {"fork",	'n',	arg_negative_flag,	&fork_flag,
     "don't fork and demonize", NULL},
#ifdef KERBEROS
    {"rxkad-level", 'r', arg_string,	&rxkad_level_string,
     "the rxkad level to use (clear, auth or crypt)", NULL},
#endif
    {"sysname",	 's',	arg_string,	&temp_sysname,
     "set the sysname of this system", NULL},
    {"root-volume",0,   arg_string,     &root_volume},
    {"port",	0,	arg_integer,	&client_port,
     "port number to use",	"number"},
    {"recover",	'z',	arg_negative_flag, &recover,
     "don't recover state",	NULL},
    {"cache-dir", 0,	arg_string,	&cache_dir,
     "cache directory",	"directory"},
    {"workers",	  0,	arg_integer,	&num_workers,
     "number of worker threads", NULL},
    {"fake-mp",	  0,	arg_flag,	&fake_mp,
     "enable fake mountpoints", NULL},
    {"cpu-usage", 0,	arg_flag,	&cpu_usage,
     NULL, NULL},
    {"version",	0,	arg_flag,	&version_flag,
     NULL, NULL},
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL}
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "[device]");
    exit (ret);
}

int
main (int argc, char **argv)
{
    int optind = 0;
    int ret;

    set_progname (argv[0]);

    if (getarg (args, sizeof(args)/sizeof(*args), argc, argv, &optind))
	usage (1);

    argc -= optind;
    argv += optind;

    if (help_flag)
	usage (0);

    if (version_flag)
	errx (1, "%s", arla_version);
    
    if (argc > 0) {
	device_file = *argv;
	argc--;
	argv++;
    }

    if (argc != 0)
	usage (1);

    if (!fork_flag)
	default_log_file = "/dev/stderr";

    if (fork_flag)
	daemonify ();

    ret = arla_init(argc, argv);
    if (ret)
	return ret;

    arla_start (device_file, cache_dir);
    
    return 0;
}
