/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <des.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfserrors.h"
#include <grp.h>

extern char *optarg;
extern int optind;
char *rmkey_usage=
"usage: tcfsrmkey [-f fliesystem-label][-g group][-p mount-point]\n";

int
rmkey_main(int argc, char *argv[])
{
	char *fs;
	uid_t uid;
	gid_t gid;
	int es;
	char x;
	char fslabel[MAXPATHLEN], fspath[MAXPATHLEN];
	int havempname = FALSE, havefsname = FALSE, isgroupkey = FALSE;
	int havefs = FALSE;
	int havename = FALSE, havefspath = FALSE;

	while ((x = getopt(argc,argv,"f:p:g:")) != EOF) {
		switch(x) {
		case 'p':
			havempname = TRUE;
			strlcpy(fspath, optarg, sizeof(fspath));
			break;
		case 'f':
			havefsname = TRUE;
			strlcpy(fslabel, optarg, sizeof(fslabel));
			break;
		case 'g':
			isgroupkey = TRUE;
			gid = atoi(optarg);
			if (!gid && optarg[0] != 0) {
				struct group *grp;
				grp = (struct group *)getgrnam(optarg);
				if (!grp)
					tcfs_error(ER_CUSTOM, 
						   "Nonexistant group\n");
				gid = grp->gr_gid;
			}
			break;
		default: 
			tcfs_error(ER_CUSTOM, rmkey_usage);
			exit(ER_UNKOPT);
		}
	}
	if (argc-optind)
		tcfs_error(ER_UNKOPT,NULL);

	if (havefsname && havempname) {
		tcfs_error(ER_CUSTOM, rmkey_usage);
		exit(1);
	}
			 
	if (havefsname) {
		es = tcfs_getfspath(fslabel, fspath);
		havename = TRUE;
	}

	if (havefspath)
		havename = TRUE;

	if (!havename)
		es = tcfs_getfspath("default",fspath);

	if(!es) {
		tcfs_error(ER_CUSTOM,"fs-label not found!\n");
		exit(1);
	}
		
	uid = getuid();

	if (isgroupkey) {
		es = tcfs_group_disable(fspath,uid,gid);
		if(es == -1)
			tcfs_error(ER_CUSTOM, "problems updating filesystem");
		exit(0);
	}

	es = tcfs_user_disable(fspath,uid);

	if (es == -1)
		tcfs_error(ER_CUSTOM,"problems updating filesystem");

	exit(0);
}
