/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Entries file to Files file
 * 
 * Creates the file Files containing the names that comprise the project, from
 * the Entries file.
 */

#include "cvs.h"

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)entries.c 1.44 94/10/07 $";
USE(rcsid);
#endif

static Node *AddEntryNode PROTO((List * list, char *name, char *version,
			   char *timestamp, char *options, char *tag,
			   char *date, char *conflict));

static FILE *entfile;
static char *entfilename;		/* for error messages */

/*
 * Write out the line associated with a node of an entries file
 */
static int write_ent_proc PROTO ((Node *, void *));
static int
write_ent_proc (node, closure)
     Node *node;
     void *closure;
{
    Entnode *p;

    p = (Entnode *) node->data;
    if (fprintf (entfile, "/%s/%s/%s", node->key, p->version,
		 p->timestamp) == EOF)
	error (1, errno, "cannot write %s", entfilename);
    if (p->conflict)
    {
	if (fprintf (entfile, "+%s", p->conflict) < 0)
	    error (1, errno, "cannot write %s", entfilename);
    }
    if (fprintf (entfile, "/%s/", p->options) < 0)
	error (1, errno, "cannot write %s", entfilename);

    if (p->tag)
    {
	if (fprintf (entfile, "T%s\n", p->tag) < 0)
	    error (1, errno, "cannot write %s", entfilename);
    }
    else if (p->date)
    {
	if (fprintf (entfile, "D%s\n", p->date) < 0)
	    error (1, errno, "cannot write %s", entfilename);
    }
    else if (fprintf (entfile, "\n") < 0)
	error (1, errno, "cannot write %s", entfilename);
    return (0);
}

/*
 * write out the current entries file given a list,  making a backup copy
 * first of course
 */
static void
write_entries (list)
    List *list;
{
    /* open the new one and walk the list writing entries */
    entfilename = CVSADM_ENTBAK;
    entfile = open_file (entfilename, "w+");
    (void) walklist (list, write_ent_proc, NULL);
    if (fclose (entfile) == EOF)
	error (1, errno, "error closing %s", entfilename);

    /* now, atomically (on systems that support it) rename it */
    rename_file (entfilename, CVSADM_ENT);

    /* now, remove the log file */
    unlink_file (CVSADM_ENTLOG);
}

/*
 * Removes the argument file from the Entries file if necessary.
 */
void
Scratch_Entry (list, fname)
    List *list;
    char *fname;
{
    Node *node;

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> Scratch_Entry(%s)\n",
			(server_active) ? 'S' : ' ', fname);
#else
	(void) fprintf (stderr, "-> Scratch_Entry(%s)\n", fname);
#endif

    /* hashlookup to see if it is there */
    if ((node = findnode (list, fname)) != NULL)
    {
	delnode (node);			/* delete the node */
#ifdef SERVER_SUPPORT
	if (server_active)
	    server_scratch (fname);
#endif
	if (!noexec)
	    write_entries (list);	/* re-write the file */
    }
}

/*
 * Enters the given file name/version/time-stamp into the Entries file,
 * removing the old entry first, if necessary.
 */
void
Register (list, fname, vn, ts, options, tag, date, ts_conflict)
    List *list;
    char *fname;
    char *vn;
    char *ts;
    char *options;
    char *tag;
    char *date;
    char *ts_conflict;
{
    Node *node;

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	server_register (fname, vn, ts, options, tag, date, ts_conflict);
    }
#endif

    if (trace)
    {
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> Register(%s, %s, %s%s%s, %s, %s %s)\n",
			(server_active) ? 'S' : ' ',
			fname, vn, ts ? ts : "",
			ts_conflict ? "+" : "", ts_conflict ? ts_conflict : "",
			options, tag ? tag : "", date ? date : "");
#else
	(void) fprintf (stderr, "-> Register(%s, %s, %s%s%s, %s, %s %s)\n",
			fname, vn, ts ? ts : "",
			ts_conflict ? "+" : "", ts_conflict ? ts_conflict : "",
			options, tag ? tag : "", date ? date : "");
#endif
    }

    node = AddEntryNode (list, fname, vn, ts, options, tag, date, ts_conflict);

    if (!noexec)
    {
	entfile = open_file (CVSADM_ENTLOG, "a");
	
	write_ent_proc (node, NULL);

        if (fclose (entfile) == EOF)
            error (1, errno, "error closing %s", CVSADM_ENTLOG);
    }
}

/*
 * Node delete procedure for list-private sticky dir tag/date info
 */
static void
freesdt (p)
    Node *p;
{
    struct stickydirtag *sdtp;

    sdtp = (struct stickydirtag *) p->data;
    if (sdtp->tag)
	free (sdtp->tag);
    if (sdtp->date)
	free (sdtp->date);
    if (sdtp->options)
	free (sdtp->options);
    free ((char *) sdtp);
}

struct entent {
    char *user;
    char *vn;
    char *ts;
    char *options;
    char *tag;
    char *date;
    char *ts_conflict;
};

struct entent *
fgetentent(fpin)
    FILE *fpin;
{
    static struct entent ent;
    static char line[MAXLINELEN];
    register char *cp;
    char *user, *vn, *ts, *options;
    char *tag_or_date, *tag, *date, *ts_conflict;

    while (fgets (line, sizeof (line), fpin) != NULL)
    {
	if (line[0] != '/')
	    continue;

	user = line + 1;
	if ((cp = strchr (user, '/')) == NULL)
	    continue;
	*cp++ = '\0';
	vn = cp;
	if ((cp = strchr (vn, '/')) == NULL)
	    continue;
	*cp++ = '\0';
	ts = cp;
	if ((cp = strchr (ts, '/')) == NULL)
	    continue;
	*cp++ = '\0';
	options = cp;
	if ((cp = strchr (options, '/')) == NULL)
	    continue;
	*cp++ = '\0';
	tag_or_date = cp;
	if ((cp = strchr (tag_or_date, '\n')) == NULL)
	    continue;
	*cp = '\0';
	tag = (char *) NULL;
	date = (char *) NULL;
	if (*tag_or_date == 'T')
	    tag = tag_or_date + 1;
	else if (*tag_or_date == 'D')
	    date = tag_or_date + 1;
	
	if ((ts_conflict = strchr (ts, '+')))
	    *ts_conflict++ = '\0';
	    
	/*
	 * XXX - Convert timestamp from old format to new format.
	 *
	 * If the timestamp doesn't match the file's current
	 * mtime, we'd have to generate a string that doesn't
	 * match anyways, so cheat and base it on the existing
	 * string; it doesn't have to match the same mod time.
	 *
	 * For an unmodified file, write the correct timestamp.
	 */
	{
	    struct stat sb;
	    if (strlen (ts) > 30 && stat (user, &sb) == 0)
	    {
		extern char *ctime ();
		char *c = ctime (&sb.st_mtime);
		
		if (!strncmp (ts + 25, c, 24))
		    ts = time_stamp (user);
		else
		{
		    ts += 24;
		    ts[0] = '*';
		}
	    }
	}

	ent.user = user;
	ent.vn = vn;
	ent.ts = ts;
	ent.options = options;
	ent.tag = tag;
	ent.date = date;
	ent.ts_conflict = ts_conflict;

	return &ent;
    }

    return NULL;
}


/*
 * Read the entries file into a list, hashing on the file name.
 */
List *
Entries_Open (aflag)
    int aflag;
{
    List *entries;
    struct entent *ent;
    char *dirtag, *dirdate;
    int do_rewrite = 0;
    FILE *fpin;

    /* get a fresh list... */
    entries = getlist ();

    /*
     * Parse the CVS/Tag file, to get any default tag/date settings. Use
     * list-private storage to tuck them away for Version_TS().
     */
    ParseTag (&dirtag, &dirdate);
    if (aflag || dirtag || dirdate)
    {
	struct stickydirtag *sdtp;

	sdtp = (struct stickydirtag *) xmalloc (sizeof (*sdtp));
	memset ((char *) sdtp, 0, sizeof (*sdtp));
	sdtp->aflag = aflag;
	sdtp->tag = xstrdup (dirtag);
	sdtp->date = xstrdup (dirdate);

	/* feed it into the list-private area */
	entries->list->data = (char *) sdtp;
	entries->list->delproc = freesdt;
    }

    fpin = fopen (CVSADM_ENT, "r");
    if (fpin == NULL)
	error (0, errno, "cannot open %s for reading", CVSADM_ENT);
    else
    {
	while ((ent = fgetentent (fpin)) != NULL) 
	{
	    (void) AddEntryNode (entries, 
				 ent->user,
				 ent->vn,
				 ent->ts,
				 ent->options,
				 ent->tag,
				 ent->date,
				 ent->ts_conflict);
	}

	fclose (fpin);
    }

    fpin = fopen (CVSADM_ENTLOG, "r");
    if (fpin != NULL) {
	while ((ent = fgetentent (fpin)) != NULL) 
	{
	    (void) AddEntryNode (entries, 
				 ent->user,
				 ent->vn,
				 ent->ts,
				 ent->options,
				 ent->tag,
				 ent->date,
				 ent->ts_conflict);
	}
	do_rewrite = 1;
	fclose (fpin);
    }

    if (do_rewrite && !noexec)
	write_entries (entries);

    /* clean up and return */
    if (fpin)
	(void) fclose (fpin);
    if (dirtag)
	free (dirtag);
    if (dirdate)
	free (dirdate);
    return (entries);
}

void
Entries_Close(list)
    List *list;
{
    if (list)
    {
	if (!noexec) 
        {
            if (isfile (CVSADM_ENTLOG))
		write_entries (list);
	}
	dellist(&list);
    }
}


/*
 * Free up the memory associated with the data section of an ENTRIES type
 * node
 */
static void
Entries_delproc (node)
    Node *node;
{
    Entnode *p;

    p = (Entnode *) node->data;
    free (p->version);
    free (p->timestamp);
    free (p->options);
    if (p->tag)
	free (p->tag);
    if (p->date)
	free (p->date);
    if (p->conflict)
	free (p->conflict);
    free ((char *) p);
}

/*
 * Get an Entries file list node, initialize it, and add it to the specified
 * list
 */
static Node *
AddEntryNode (list, name, version, timestamp, options, tag, date, conflict)
    List *list;
    char *name;
    char *version;
    char *timestamp;
    char *options;
    char *tag;
    char *date;
    char *conflict;
{
    Node *p;
    Entnode *entdata;

    /* was it already there? */
    if ((p  = findnode (list, name)) != NULL)
    {
	/* take it out */
	delnode (p);
    }

    /* get a node and fill in the regular stuff */
    p = getnode ();
    p->type = ENTRIES;
    p->delproc = Entries_delproc;

    /* this one gets a key of the name for hashing */
    p->key = xstrdup (name);

    /* malloc the data parts and fill them in */
    p->data = xmalloc (sizeof (Entnode));
    entdata = (Entnode *) p->data;
    entdata->version = xstrdup (version);
    entdata->timestamp = xstrdup (timestamp);
    if (entdata->timestamp == NULL)
       entdata->timestamp = xstrdup ("");/* must be non-NULL */
    entdata->options = xstrdup (options);
    if (entdata->options == NULL)
	entdata->options = xstrdup ("");/* must be non-NULL */
    entdata->conflict = xstrdup (conflict);
    entdata->tag = xstrdup (tag);
    entdata->date = xstrdup (date);

    /* put the node into the list */
    addnode (list, p);
    return (p);
}

/*
 * Write out/Clear the CVS/Tag file.
 */
void
WriteTag (dir, tag, date)
    char *dir;
    char *tag;
    char *date;
{
    FILE *fout;
    char tmp[PATH_MAX];

    if (noexec)
	return;

    if (dir == NULL)
	(void) strcpy (tmp, CVSADM_TAG);
    else
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_TAG);

    if (tag || date)
    {
	fout = open_file (tmp, "w+");
	if (tag)
	{
	    if (fprintf (fout, "T%s\n", tag) < 0)
		error (1, errno, "write to %s failed", tmp);
	}
	else
	{
	    if (fprintf (fout, "D%s\n", date) < 0)
		error (1, errno, "write to %s failed", tmp);
	}
	if (fclose (fout) == EOF)
	    error (1, errno, "cannot close %s", tmp);
    }
    else
	if (unlink_file (tmp) < 0 && errno != ENOENT)
	    error (1, errno, "cannot remove %s", tmp);
}

/*
 * Parse the CVS/Tag file for the current directory.
 */
void
ParseTag (tagp, datep)
    char **tagp;
    char **datep;
{
    FILE *fp;
    char line[MAXLINELEN];
    char *cp;

    if (tagp)
	*tagp = (char *) NULL;
    if (datep)
	*datep = (char *) NULL;
    fp = fopen (CVSADM_TAG, "r");
    if (fp)
    {
	if (fgets (line, sizeof (line), fp) != NULL)
	{
	    if ((cp = strrchr (line, '\n')) != NULL)
		*cp = '\0';
	    if (*line == 'T' && tagp)
		*tagp = xstrdup (line + 1);
	    else if (*line == 'D' && datep)
		*datep = xstrdup (line + 1);
	}
	(void) fclose (fp);
    }
}
