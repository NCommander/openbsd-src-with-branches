/*	$OpenBSD$ */

/*
 * Copyright (c) 1996 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD$";
#endif

#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netdb.h>

struct user {
	char	*usr_name;		/* user name */
	int	usr_uid;		/* user uid */
	int	usr_gid;		/* user gid */
	int	gid_count;		/* number of gids */
	int	gid[NGROUPS];		/* additional gids */
	struct user *prev,*next;	/* links in read order */
	struct user *hprev,*hnext;	/* links in hash order */
};

#ifdef HOSTS
char *HostFile = HOSTS;
#else
char *HostFile = _PATH_HOSTS;
#endif

#ifdef PASSWD
char *PasswdFile = PASSWD;
#else
char *PasswdFile = _PATH_PASSWD;
#endif

#ifdef GROUP
char *GroupFile = GROUP;
#else
char *GroupFile = _PATH_GROUP;
#endif

#ifdef NETID
char *NetidFile = NETID;
#else
char *NetidFile = "/etc/netid";
#endif

#define HASHMAX 55

char *ProgramName = "mknetid";
struct user *root = NULL, *tail = NULL;
struct user *hroot[HASHMAX], *htail[HASHMAX];

static int read_line(fp, buf, size)
	FILE *fp;
	char *buf;
	int size;
{
	int done = 0;

	do {
		while (fgets(buf, size, fp)) {
			int len = strlen(buf);
			done += len;
			if (len > 1 && buf[len-2] == '\\' &&
					buf[len-1] == '\n') {
				int ch;
				buf += len - 2;
				size -= len - 2;
				*buf = '\n'; buf[1] = '\0';
				/*
				 * Skip leading white space on next line
				 */
				while ((ch = getc(fp)) != EOF &&
					isascii(ch) && isspace(ch))
						;
				(void) ungetc(ch, fp);
			} else {
				return done;
			}
		}
	} while (size > 0 && !feof(fp));

	return done;
}

int
hashidx(key)
char key;
{
	if (key < 'A')
		return(0);
	if (key <= 'Z')
		return(1 + key - 'A');
	if (key < 'a')
		return(27);
	if (key <= 'z')
		return(28 + key - 'a');
	return(54);
}

void add_user(username, uid, gid)
char *username, *uid, *gid;
{
	struct user *u;
	int idx;

	u = (struct user *) malloc(sizeof(struct user));
	bzero((void *) u,sizeof(struct user));
	u->usr_name = (char *) malloc(strlen(username) + 1);
	strcpy(u->usr_name,username);
	u->usr_uid=atoi(uid);
	u->usr_gid=atoi(gid);
	u->gid_count=-1;
	if (root == NULL) {
		root = tail = u;
	} else {
		u->prev = tail;
		tail->next = u;
		tail = u;
	}
	idx = hashidx(username[0]);
	if (hroot[idx] == NULL) {
		hroot[idx] = htail[idx] = u;
	} else {
		u->hprev = htail[idx];
		htail[idx]->hnext = u;
		htail[idx] = u;
	}
}

void add_group(username, gid)
char *username, *gid;
{
	struct user *u;
	int idx, g;

	idx = hashidx(username[0]);
	u = hroot[idx];
	g = atoi(gid);
	
	while (u != NULL) {
		if (strcmp(username,u->usr_name) == 0) {
			if (g != u->usr_gid) {
				u->gid_count++;
				if (u->gid_count < NGROUPS) {
					u->gid[u->gid_count]=atoi(gid);
				}
			}
			u = htail[idx];
		}
		u = u->hnext;
	}
}

void
read_passwd(pfile, fname)
FILE *pfile;
char *fname;
{
	char  line[1024];
	int   line_no = 0;
	int   len, colon;
	char  *p,*k,*u,*g;

	while (read_line(pfile,line,sizeof(line))) {
		
		line_no++;
		len = strlen(line);
		
		if (len > 0) {
			if (line[0] == '#')
				continue;
		}

		/*
		 * Check if we have the whole line
		 */ 
		
		if (line[len-1] != '\n') {
			fprintf(stderr,
				"line %d in \"%s\" is too long\n",
				line_no, fname);
		} else {
			line[len-1] = '\0';
		}

		p = (char *) &line;
		
		k = p; colon = 0;
		while (*k != '\0') {
			if (*k == ':')
				colon++;
			k++;
		}

		if (colon > 0) {
			k = p;			     /* save start of key  */
			while (*p != ':') { p++; };  /* find first "colon" */
			if (*p==':') { *p = '\0'; p++; }; /* terminate key */
			if (strlen(k) == 1) {
				if (*k == '+')
					continue;
			}
		}
		
		if (colon < 4) {
			fprintf(stderr,
				"syntax error at line %d in \"%s\"\n",
				line_no, fname);
			continue;
		}

		while (*p != ':') { p++; };	/* find second "colon" */
		if (*p==':') { *p = '\0'; p++; }; /* terminate passwd */
		u = p;
		while (*p != ':') { p++; };	/* find third "colon" */
		if (*p==':') { *p = '\0'; p++; }; /* terminate uid */
		g = p;
		while (*p != ':') { p++; };	/* find forth "colon" */
		if (*p==':') { *p = '\0'; p++; }; /* terminate gid */
		while(*p != '\0') { p++; };	/* find end of string */
		
		add_user(k,u,g);
	}
}

int
isgsep(ch)
char ch;
{
	int ret;

	switch(ch)  {
	case ',':
	case ' ':
	case '\t':
	case '\0':
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

void
read_group(gfile, fname)
FILE *gfile;
char *fname;
{
	char  line[2048];
	int   line_no = 0;
	int   len, colon;
	char  *p,*k,*u,*g;

	while (read_line(gfile,line,sizeof(line))) {
		
		line_no++;
		len = strlen(line);
		
		if (len > 0) {
			if (line[0] == '#')
				continue;
		}

		/*
		 * Check if we have the whole line
		 */ 
		
		if (line[len-1] != '\n') {
			fprintf(stderr,
				"line %d in \"%s\" is too long\n",
				line_no, fname);
		} else {
			line[len-1] = '\0';
		}

		p = (char *) &line;
		
		k = p; colon = 0;
		while (*k != '\0') {
			if (*k == ':')
				colon++;
			k++;
		}

		if (colon > 0) {
			k = p;			     /* save start of key  */
			while (*p != ':') { p++; };  /* find first "colon" */
			if (*p==':') { *p = '\0'; p++; }; /* terminate key */
			if (strlen(k) == 1) {
				if (*k == '+')
					continue;
			}
		}
		
		if (colon < 3) {
			fprintf(stderr,
				"syntax error at line %d in \"%s\"\n",
				line_no, fname);
			continue;
		}

		while (*p != ':') { p++; };	/* find second "colon" */
		if (*p==':') { *p = '\0'; p++; }; /* terminate passwd */
		g = p;
		while (*p != ':') { p++; };	/* find third "colon" */
		if (*p==':') { *p = '\0'; p++; }; /* terminate gid */

		u = p;

		while (*u != '\0') {
			while (!isgsep(*p)) { p++; };	/* find separator */
			if (*p != '\0') {
				*p = '\0';
				if (u != p) {
					add_group(u,g);
				}
				p++;
			} else {
				if (u != p) {
					add_group(u,g);
				}
			}
			u = p;
		}
	}
}

void
print_passwd_group(qflag,domain)
int qflag;
char *domain;
{
	struct user *u,*p;
	int i;

	u = root;

	while (u != NULL) {
		p = root;
		while(p->usr_uid != u->usr_uid) {
			p = p->next;
		}
		if (p != u) {
			if (!qflag) {
				fprintf(stderr,
					"%s: unix.%d@%s %s\n",
					ProgramName,
					u->usr_uid,
					domain,
					"multiply defined, other definitions ignored");
			}
		} else {
			printf("unix.%d@%s %d:%d",
				u->usr_uid,
				domain,
			       u->usr_uid,
			       u->usr_gid);
			if (u->gid_count >= 0) {
				i=0;
				while (i <= u->gid_count) {
					printf(",%d",u->gid[i]);
					i++;
				}
			}
			printf("\n");
		}
		u = u->next;
	}
}

void
print_hosts(pfile, fname, domain)
FILE *pfile;
char *fname, *domain;
{
	char  line[1024];
	int   line_no = 0;
	int   len, colon;
	char  *p,*k,*u;

	while (read_line(pfile,line,sizeof(line))) {
		
		line_no++;
		len = strlen(line);
		
		if (len > 0) {
			if (line[0] == '#')
				continue;
		}

		/*
		 * Check if we have the whole line
		 */ 
		
		if (line[len-1] != '\n') {
			fprintf(stderr,
				"line %d in \"%s\" is too long\n",
				line_no, fname);
		} else {
			line[len-1] = '\0';
		}

		p = (char *) &line;
		
		k = p;				/* save start of key  */
		while (!isspace(*p)) { p++; };	/* find first "space" */
		while (isspace(*p)) {		/* replace space with <NUL> */
			*p = '\0';
			p++;
		}

		u = p;
		while (p != NULL) {
			if (*p == '\0') {
				p = NULL;
			} else {
				if (!isspace(*p)) {
					p++;
				} else {
					*p = '\0';
					p = NULL;
				}
			}
		}

		printf("unix.%s@%s 0:%s\n",
		       u,
		       domain,
		       u);
	}
}

void
print_netid(mfile, fname)
FILE *mfile;
char *fname;
{
	char  line[1024];
	int   line_no = 0;
	int   len, colon;
	char  *p,*k,*u;

	while (read_line(mfile,line,sizeof(line))) {
		
		line_no++;
		len = strlen(line);
		
		if (len > 0) {
			if (line[0] == '#')
				continue;
		}

		/*
		 * Check if we have the whole line
		 */ 
		
		if (line[len-1] != '\n') {
			fprintf(stderr,
				"line %d in \"%s\" is too long\n",
				line_no, fname);
		} else {
			line[len-1] = '\0';
		}

		p = (char *) &line;
		
		k = p;				/* save start of key  */
		while (!isspace(*p)) { p++; };	/* find first "space" */
		while (isspace(*p)) {		/* replace space with <NUL> */
			*p = '\0';
			p++;
		}

		u = p;
		while (p != NULL) {
			if (*p == '\0') {
				p = NULL;
			} else {
				if (!isspace(*p)) {
					p++;
				} else {
					*p = '\0';
					p = NULL;
				}
			}
		}

		printf("%s %s\n",k,u);
	}
}

int
main (argc,argv)
int argc;
char *argv[];
{
	int	qflag, ch, usage;
	char   *domain;
	FILE   *pfile, *gfile, *hfile, *mfile;

	qflag = usage = 0;
	domain = NULL;
	
	while ((ch = getopt(argc, argv, "d:g:h:m:p:q")) != EOF)
		switch (ch) {
			case 'd':
				domain = optarg;
				break;
			case 'g':
				GroupFile = optarg;
				break;
			case 'h':
				HostFile = optarg;
				break;
			case 'm':
				NetidFile = optarg;
				break;
			case 'p':
				PasswdFile = optarg;
				break;
			case 'q':
				qflag++;
				break;
			default:
				usage++;
				break;
		}
	
	if (argc > optind) {
	  usage++;
	}

	if (usage) {
		fprintf(stderr,
			"usage:\t%s [-d domain] [-q] [-p passwdfile] [-g groupfile]\n\t\t[-h hostfile] [-m netidfile]\n",
			ProgramName);
		exit(1);
	}
	
	if (domain == NULL) {
		yp_get_default_domain(&domain);
	}

	pfile = fopen(PasswdFile, "r");
	if (pfile == NULL) {
		fprintf(stderr,"%s: can't open file \"%s\"\n",
			ProgramName, PasswdFile);
		exit(1);
	}

	gfile = fopen(GroupFile, "r");
	if (gfile == NULL) {
		fprintf(stderr,"%s: can't open file \"%s\"\n",
			ProgramName, PasswdFile);
		exit(1);		
	}

	hfile = fopen(HostFile, "r");
	if (hfile == NULL) {
		fprintf(stderr,"%s: can't open file \"%s\"\n",
			ProgramName, PasswdFile);
		exit(1);
	}

	printf("NetidFile: %s\n",NetidFile);
	mfile = fopen(NetidFile, "r");

	read_passwd(pfile,PasswdFile);
	read_group(gfile,GroupFile);

	print_passwd_group(qflag,domain);
	print_hosts(hfile,HostFile,domain);
	if (mfile != NULL) {
		print_netid(mfile,NetidFile);
	}

	return(0);
  
}
