/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * The routines contained in this file do all the rcs file parsing and
 * manipulation
 */

#include <assert.h>
#include "cvs.h"
#include "edit.h"

/* The RCS -k options, and a set of enums that must match the array.
   These come first so that we can use enum kflag in function
   prototypes.  */
static const char *const kflags[] =
  {"kv", "kvl", "k", "v", "o", "b", (char *) NULL};
enum kflag { KFLAG_KV = 0, KFLAG_KVL, KFLAG_K, KFLAG_V, KFLAG_O, KFLAG_B };

static RCSNode *RCS_parsercsfile_i PROTO((FILE * fp, const char *rcsfile));
static char *RCS_getdatebranch PROTO((RCSNode * rcs, char *date, char *branch));
static int getrcskey PROTO((FILE * fp, char **keyp, char **valp,
			    size_t *lenp));
static void getrcsrev PROTO ((FILE *fp, char **revp));
static int checkmagic_proc PROTO((Node *p, void *closure));
static void do_branches PROTO((List * list, char *val));
static void do_symbols PROTO((List * list, char *val));
static void do_locks PROTO((List * list, char *val));
static void free_rcsnode_contents PROTO((RCSNode *));
static void free_rcsvers_contents PROTO((RCSVers *));
static void rcsvers_delproc PROTO((Node * p));
static char *translate_symtag PROTO((RCSNode *, const char *));
static char *printable_date PROTO((const char *));
static char *escape_keyword_value PROTO ((const char *, int *));
static void expand_keywords PROTO((RCSNode *, RCSVers *, const char *,
				   const char *, size_t, enum kflag, char *,
				   size_t, char **, size_t *));
static void cmp_file_buffer PROTO((void *, const char *, size_t));

enum rcs_delta_op {RCS_ANNOTATE, RCS_FETCH};
static void RCS_deltas PROTO ((RCSNode *, FILE *, char *, enum rcs_delta_op,
			       char **, size_t *, char **, size_t *));

/* Routines for reading, parsing and writing RCS files. */
static RCSVers *getdelta PROTO ((FILE *, char *));
static Deltatext *RCS_getdeltatext PROTO ((RCSNode *, FILE *));
static void freedeltatext PROTO ((Deltatext *));

static void RCS_putadmin PROTO ((RCSNode *, FILE *));
static void RCS_putdtree PROTO ((RCSNode *, char *, FILE *));
static void RCS_putdesc PROTO ((RCSNode *, FILE *));
static void putdelta PROTO ((RCSVers *, FILE *));
static int putrcsfield_proc PROTO ((Node *, void *));
static int putsymbol_proc PROTO ((Node *, void *));
static void RCS_copydeltas PROTO ((RCSNode *, FILE *, FILE *, Deltatext *, char *));
static void putdeltatext PROTO ((FILE *, Deltatext *));

static FILE *rcs_internal_lockfile PROTO ((char *));
static void rcs_internal_unlockfile PROTO ((FILE *, char *));
static char *rcs_lockfilename PROTO ((char *));

/*
 * We don't want to use isspace() from the C library because:
 *
 * 1. The definition of "whitespace" in RCS files includes ASCII
 *    backspace, but the C locale doesn't.
 * 2. isspace is an very expensive function call in some implementations
 *    due to the addition of wide character support.
 */
static const char spacetab[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0,	/* 0x00 - 0x0f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 - 0x1f */
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x20 - 0x2f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x3f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x4f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x5f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x8f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x7f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80 - 0x8f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x90 - 0x9f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xa0 - 0xaf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xb0 - 0xbf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xc0 - 0xcf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xd0 - 0xdf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xe0 - 0xef */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* 0xf0 - 0xff */
};

#define whitespace(c)	(spacetab[(unsigned char)c] != 0)


/* Parse an rcsfile given a user file name and a repository.  If there is
   an error, we print an error message and return NULL.  If the file
   does not exist, we return NULL without printing anything (I'm not
   sure this allows the caller to do anything reasonable, but it is
   the current behavior).  */
RCSNode *
RCS_parse (file, repos)
    const char *file;
    const char *repos;
{
    RCSNode *rcs;
    FILE *fp;
    RCSNode *retval;
    char *rcsfile;

    rcsfile = xmalloc (strlen (repos) + strlen (file)
		       + sizeof (RCSEXT) + sizeof (CVSATTIC) + 10);
    (void) sprintf (rcsfile, "%s/%s%s", repos, file, RCSEXT);
    if ((fp = CVS_FOPEN (rcsfile, FOPEN_BINARY_READ)) != NULL) 
    {
        rcs = RCS_parsercsfile_i(fp, rcsfile);
	if (rcs != NULL) 
	    rcs->flags |= VALID;

	fclose (fp);
	retval = rcs;
	goto out;
    }
    else if (! existence_error (errno))
    {
	error (0, errno, "cannot open %s", rcsfile);
	retval = NULL;
	goto out;
    }

    (void) sprintf (rcsfile, "%s/%s/%s%s", repos, CVSATTIC, file, RCSEXT);
    if ((fp = CVS_FOPEN (rcsfile, FOPEN_BINARY_READ)) != NULL) 
    {
        rcs = RCS_parsercsfile_i(fp, rcsfile);
	if (rcs != NULL)
	{
	    rcs->flags |= INATTIC;
	    rcs->flags |= VALID;
	}

	fclose (fp);
	retval = rcs;
	goto out;
    }
    else if (! existence_error (errno))
    {
	error (0, errno, "cannot open %s", rcsfile);
	retval = NULL;
	goto out;
    }
#if defined (SERVER_SUPPORT) && !defined (FILENAMES_CASE_INSENSITIVE)
    else if (ign_case)
    {
	int status;
	char *found_path;

	/* The client might be asking for a file which we do have
	   (which the client doesn't know about), but for which the
	   filename case differs.  We only consider this case if the
	   regular CVS_FOPENs fail, because fopen_case is such an
	   expensive call.  */
	(void) sprintf (rcsfile, "%s/%s%s", repos, file, RCSEXT);
	status = fopen_case (rcsfile, "rb", &fp, &found_path);
	if (status == 0)
	{
	    rcs = RCS_parsercsfile_i (fp, rcsfile);
	    if (rcs != NULL) 
		rcs->flags |= VALID;

	    fclose (fp);
	    free (rcs->path);
	    rcs->path = found_path;
	    retval = rcs;
	    goto out;
	}
	else if (! existence_error (status))
	{
	    error (0, status, "cannot open %s", rcsfile);
	    retval = NULL;
	    goto out;
	}

	(void) sprintf (rcsfile, "%s/%s/%s%s", repos, CVSATTIC, file, RCSEXT);
	status = fopen_case (rcsfile, "rb", &fp, &found_path);
	if (status == 0)
	{
	    rcs = RCS_parsercsfile_i (fp, rcsfile);
	    if (rcs != NULL)
	    {
		rcs->flags |= INATTIC;
		rcs->flags |= VALID;
	    }

	    fclose (fp);
	    free (rcs->path);
	    rcs->path = found_path;
	    retval = rcs;
	    goto out;
	}
	else if (! existence_error (status))
	{
	    error (0, status, "cannot open %s", rcsfile);
	    retval = NULL;
	    goto out;
	}
    }
#endif
    retval = NULL;

 out:
    free (rcsfile);

    return retval;
}

/*
 * Parse a specific rcsfile.
 */
RCSNode *
RCS_parsercsfile (rcsfile)
    char *rcsfile;
{
    FILE *fp;
    RCSNode *rcs;

    /* open the rcsfile */
    if ((fp = CVS_FOPEN (rcsfile, FOPEN_BINARY_READ)) == NULL)
    {
	error (0, errno, "Couldn't open rcs file `%s'", rcsfile);
	return (NULL);
    }

    rcs = RCS_parsercsfile_i (fp, rcsfile);

    fclose (fp);
    return (rcs);
}


/*
 */ 
static RCSNode *
RCS_parsercsfile_i (fp, rcsfile)
    FILE *fp;
    const char *rcsfile;
{
    RCSNode *rdata;
    char *key, *value;

    /* make a node */
    rdata = (RCSNode *) xmalloc (sizeof (RCSNode));
    memset ((char *) rdata, 0, sizeof (RCSNode));
    rdata->refcount = 1;
    rdata->path = xstrdup (rcsfile);

    /* Process HEAD and BRANCH keywords from the RCS header.  

       Most cvs operations on the main branch don't need any more
       information.  Those that do call RCS_reparsercsfile to parse
       the rest of the header and the deltas.

       People often wonder whether this is inefficient, to open the
       file once here and once in RCS_reparsercsfile.  Well, it might
       help a little bit if we kept the file open (I haven't tried
       timing this myself), but basically the common case, which we
       want to optimize, is the one in which we call
       RCS_parsercsfile_i and not RCS_reparsercsfile (for example,
       "cvs update" on a lot of files most of which are unmodified).
       So making the case in which we call RCS_reparsercsfile fast is
       not as important.  */

    if (getrcskey (fp, &key, &value, NULL) == -1 || key == NULL)
	goto l_error;
    if (strcmp (key, RCSDESC) == 0)
	goto l_error;

    if (strcmp (RCSHEAD, key) == 0 && value != NULL)
	rdata->head = xstrdup (value);

    if (getrcskey (fp, &key, &value, NULL) == -1 || key == NULL)
	goto l_error;
    if (strcmp (key, RCSDESC) == 0)
	goto l_error;

    if (strcmp (RCSBRANCH, key) == 0 && value != NULL)
    {
	char *cp;

	rdata->branch = xstrdup (value);
	if ((numdots (rdata->branch) & 1) != 0)
	{
	    /* turn it into a branch if it's a revision */
	    cp = strrchr (rdata->branch, '.');
	    *cp = '\0';
	}
    }

    rdata->flags |= PARTIAL;
    return rdata;

l_error:
    if (!really_quiet)
    {
	if (ferror(fp))
	{
	    error (1, 0, "error reading `%s'", rcsfile);
	}
	else
	{
	    error (0, 0, "`%s' does not appear to be a valid rcs file",
		   rcsfile);
	}
    }
    freercsnode (&rdata);
    return (NULL);
}


/* Do the real work of parsing an RCS file.

   On error, die with a fatal error; if it returns at all it was successful.

   If PFP is NULL, close the file when done.  Otherwise, leave it open
   and store the FILE * in *PFP.  */
void
RCS_reparsercsfile (rdata, pfp)
    RCSNode *rdata;
    FILE **pfp;
{
    FILE *fp;
    char *rcsfile;

    Node *q, *kv;
    RCSVers *vnode;
    long fpos;
    char *cp;
    char *key, *value;

    assert (rdata != NULL);
    rcsfile = rdata->path;

    fp = CVS_FOPEN (rcsfile, FOPEN_BINARY_READ);
    if (fp == NULL)
	error (1, 0, "unable to reopen `%s'", rcsfile);

    /* make a node */
    /* This probably shouldn't be done until later: if a file has an
       empty revision tree (which is permissible), rdata->versions
       should be NULL. -twp */
    rdata->versions = getlist ();

    /*
     * process all the special header information, break out when we get to
     * the first revision delta
     */
    for (;;)
    {
	fpos = ftell (fp);

	/* get the next key/value pair */

	/* if key is NULL here, then the file is missing some headers
	   or we had trouble reading the file. */
	if (getrcskey (fp, &key, &value, NULL) == -1 || key == NULL)
	{
	    if (ferror(fp))
	    {
		error (1, 0, "error reading `%s'", rcsfile);
	    }
	    else
	    {
		error (1, 0, "`%s' does not appear to be a valid rcs file",
		       rcsfile);
	    }
	}

	/* Skip head and branch tags; we already have them. */
	if (strcmp (key, RCSHEAD) == 0 || strcmp (key, RCSBRANCH) == 0)
	    continue;

	if (strcmp (key, "access") == 0)
	{
	    if (value != NULL)
		rdata->access = xstrdup (value);
	    continue;
	}

	/* We always save lock information, so that we can handle
           -kkvl correctly when checking out a file. */
	if (strcmp (key, "locks") == 0)
	{
	    if (value != NULL)
		rdata->locks_data = xstrdup (value);
	    fpos = ftell (fp);
	    if (getrcskey (fp, &key, &value, NULL) >= 0 &&
		strcmp (key, "strict") == 0 &&
		value == NULL)
	    {
		rdata->strict_locks = 1;
	    }
	    else
		(void) fseek (fp, fpos, SEEK_SET);
	    continue;
	}

	if (strcmp (RCSSYMBOLS, key) == 0)
	{
	    if (value != NULL)
		rdata->symbols_data = xstrdup(value);
	    continue;
	}

	if (strcmp (RCSEXPAND, key) == 0)
	{
	    rdata->expand = xstrdup (value);
	    continue;
	}

	/*
	 * check key for '.''s and digits (probably a rev) if it is a
	 * revision or `desc', we are done with the headers and are down to the
	 * revision deltas, so we break out of the loop
	 */
	for (cp = key; (isdigit (*cp) || *cp == '.') && *cp != '\0'; cp++)
	     /* do nothing */ ;
	if (*cp == '\0' && strncmp (RCSDATE, value, strlen (RCSDATE)) == 0)
	    break;

	if (strcmp (key, RCSDESC) == 0)
	    break;

	if (strcmp (key, "comment") == 0)
	{
	    rdata->comment = xstrdup (value);
	    continue;
	}
	if (rdata->other == NULL)
	    rdata->other = getlist ();
	kv = getnode ();
	kv->type = RCSFIELD;
	kv->key = xstrdup (key);
	kv->data = xstrdup (value);
	if (addnode (rdata->other, kv) != 0)
	{
	    error (0, 0, "warning: duplicate key `%s' in RCS file `%s'",
		   key, rcsfile);
	    freenode (kv);
	}

	/* if we haven't grabbed it yet, we didn't want it */
    }

    /*
     * we got out of the loop, so we have the first part of the first
     * revision delta in our hand key=the revision and value=the date key and
     * its value
     */
    /* First, seek back to the start of the delta block. */
    (void) fseek (fp, fpos, SEEK_SET);

    while ((vnode = getdelta (fp, rcsfile)) != NULL)
    {
	/* get the node */
	q = getnode ();
	q->type = RCSVERS;
	q->delproc = rcsvers_delproc;
	q->data = (char *) vnode;
	q->key = vnode->version;

	/* add the nodes to the list */
	if (addnode (rdata->versions, q) != 0)
	{
#if 0
		purify_printf("WARNING: Adding duplicate version: %s (%s)\n",
			 q->key, rcsfile);
		freenode (q);
#endif
	}
    }

    (void) getrcskey (fp, &key, &value, NULL);
    if (key != NULL && strcmp (key, RCSDESC) == 0)
    {
	if (rdata->desc != NULL)
	{
	    error (0, 0,
		   "warning: duplicate key `%s' in RCS file `%s'",
		   key, rcsfile);
	    free (rdata->desc);
	}
	rdata->desc = xstrdup (value);
    }

    rdata->delta_pos = ftell (fp);

    if (pfp == NULL)
    {
	if (fclose (fp) < 0)
	    error (0, errno, "cannot close %s", rcsfile);
    }
    else
    {
	*pfp = fp;
    }
    rdata->flags &= ~PARTIAL;
}

/*
 * Fully parse the RCS file.  Store all keyword/value pairs, fetch the
 * log messages for each revision, and fetch add and delete counts for
 * each revision (we could fetch the entire text for each revision,
 * but the only caller, log_fileproc, doesn't need that information,
 * so we don't waste the memory required to store it).  The add and
 * delete counts are stored on the OTHER field of the RCSVERSNODE
 * structure, under the names ";add" and ";delete", so that we don't
 * waste the memory space of extra fields in RCSVERSNODE for code
 * which doesn't need this information.
 */

void
RCS_fully_parse (rcs)
    RCSNode *rcs;
{
    FILE *fp;

    RCS_reparsercsfile (rcs, &fp);

    while (1)
    {
	int c;
	char *key, *value;
	size_t vallen;
	Node *vers;
	RCSVers *vnode;

	/* Rather than try to keep track of how much information we
           have read, just read to the end of the file.  */
	do
	{
	    c = getc (fp);
	    if (c == EOF)
		break;
	} while (whitespace (c));
	if (c == EOF)
	    break;
	if (ungetc (c, fp) == EOF)
	    error (1, errno, "ungetc failed");

	getrcsrev (fp, &key);
	vers = findnode (rcs->versions, key);
	if (vers == NULL)
	    error (1, 0,
		   "mismatch in rcs file %s between deltas and deltatexts",
		   rcs->path);

	vnode = (RCSVers *) vers->data;

	while (getrcskey (fp, &key, &value, &vallen) >= 0)
	{
	    if (strcmp (key, "text") != 0)
	    {
		Node *kv;

		if (vnode->other == NULL)
		    vnode->other = getlist ();
		kv = getnode ();
		kv->type = RCSFIELD;
		kv->key = xstrdup (key);
		kv->data = xstrdup (value);
		if (addnode (vnode->other, kv) != 0)
		{
		    error (0, 0,
			   "\
warning: duplicate key `%s' in version `%s' of RCS file `%s'",
			   key, vnode->version, rcs->path);
		    freenode (kv);
		}

		continue;
	    }

	    if (strcmp (vnode->version, rcs->head) != 0)
	    {
		unsigned long add, del;
		char buf[50];
		Node *kv;

		/* This is a change text.  Store the add and delete
                   counts.  */
		add = 0;
		del = 0;
		if (value != NULL)
		{
		    const char *cp;

		    cp = value;
		    while (cp < value + vallen)
		    {
			char op;
			unsigned long count;

			op = *cp++;
			if (op != 'a' && op  != 'd')
			    error (1, 0, "unrecognized operation '%c' in %s",
				   op, rcs->path);
			(void) strtoul (cp, (char **) &cp, 10);
			if (*cp++ != ' ')
			    error (1, 0, "space expected in %s",
				   rcs->path);
			count = strtoul (cp, (char **) &cp, 10);
			if (*cp++ != '\012')
			    error (1, 0, "linefeed expected in %s",
				   rcs->path);

			if (op == 'd')
			    del += count;
			else
			{
			    add += count;
			    while (count != 0)
			    {
				if (*cp == '\012')
				    --count;
				else if (cp == value + vallen)
				{
				    if (count != 1)
					error (1, 0, "\
invalid rcs file %s: premature end of value",
					       rcs->path);
				    else
					break;
				}
				++cp;
			    }
			}
		    }
		}

		sprintf (buf, "%lu", add);
		kv = getnode ();
		kv->type = RCSFIELD;
		kv->key = xstrdup (";add");
		kv->data = xstrdup (buf);
		if (addnode (vnode->other, kv) != 0)
		{
		    error (0, 0,
			   "\
warning: duplicate key `%s' in version `%s' of RCS file `%s'",
			   key, vnode->version, rcs->path);
		    freenode (kv);
		}

		sprintf (buf, "%lu", del);
		kv = getnode ();
		kv->type = RCSFIELD;
		kv->key = xstrdup (";delete");
		kv->data = xstrdup (buf);
		if (addnode (vnode->other, kv) != 0)
		{
		    error (0, 0,
			   "\
warning: duplicate key `%s' in version `%s' of RCS file `%s'",
			   key, vnode->version, rcs->path);
		    freenode (kv);
		}
	    }

	    /* We have found the "text" key which ends the data for
               this revision.  Break out of the loop and go on to the
               next revision.  */
	    break;
	}
    }

    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", rcs->path);
}

/*
 * freercsnode - free up the info for an RCSNode
 */
void
freercsnode (rnodep)
    RCSNode **rnodep;
{
    if (rnodep == NULL || *rnodep == NULL)
	return;

    ((*rnodep)->refcount)--;
    if ((*rnodep)->refcount != 0)
    {
	*rnodep = (RCSNode *) NULL;
	return;
    }
    free ((*rnodep)->path);
    if ((*rnodep)->head != (char *) NULL)
	free ((*rnodep)->head);
    if ((*rnodep)->branch != (char *) NULL)
	free ((*rnodep)->branch);
    free_rcsnode_contents (*rnodep);
    free ((char *) *rnodep);
    *rnodep = (RCSNode *) NULL;
}

/*
 * free_rcsnode_contents - free up the contents of an RCSNode without
 * freeing the node itself, or the file name, or the head, or the
 * path.  This returns the RCSNode to the state it is in immediately
 * after a call to RCS_parse.
 */
static void
free_rcsnode_contents (rnode)
    RCSNode *rnode;
{
    dellist (&rnode->versions);
    if (rnode->symbols != (List *) NULL)
	dellist (&rnode->symbols);
    if (rnode->symbols_data != (char *) NULL)
	free (rnode->symbols_data);
    if (rnode->expand != NULL)
	free (rnode->expand);
    if (rnode->other != (List *) NULL)
	dellist (&rnode->other);
    if (rnode->access != NULL)
	free (rnode->access);
    if (rnode->locks_data != NULL)
	free (rnode->locks_data);
    if (rnode->locks != (List *) NULL)
	dellist (&rnode->locks);
    if (rnode->comment != NULL)
	free (rnode->comment);
    if (rnode->desc != NULL)
	free (rnode->desc);
}

/* free_rcsvers_contents -- free up the contents of an RCSVers node,
   but also free the pointer to the node itself. */

static void
free_rcsvers_contents (rnode)
    RCSVers *rnode;
{
    if (rnode->branches != (List *) NULL)
	dellist (&rnode->branches);
    if (rnode->date != (char *) NULL)
	free (rnode->date);
    if (rnode->next != (char *) NULL)
	free (rnode->next);
    if (rnode->author != (char *) NULL)
	free (rnode->author);
    if (rnode->state != (char *) NULL)
	free (rnode->state);
    if (rnode->other != (List *) NULL)
	dellist (&rnode->other);
    if (rnode->other_delta != NULL)
	dellist (&rnode->other_delta);
    if (rnode->text != NULL)
	freedeltatext (rnode->text);
    free ((char *) rnode);
}

/*
 * rcsvers_delproc - free up an RCSVers type node
 */
static void
rcsvers_delproc (p)
    Node *p;
{
    free_rcsvers_contents ((RCSVers *) p->data);
}

/*
 * getrcskey - fill in the key and value from the rcs file the algorithm is
 *             as follows 
 *
 *    o skip whitespace
 *    o fill in key with everything up to next white 
 *      space or semicolon 
 *    o if key == "desc" then key and data are NULL and return -1 
 *    o if key wasn't terminated by a semicolon, skip white space and fill 
 *      in value with everything up to a semicolon 
 *    o compress all whitespace down to a single space 
 *    o if a word starts with @, do funky rcs processing
 *    o strip whitespace off end of value or set value to NULL if it empty 
 *    o return 0 since we found something besides "desc"
 *
 * Sets *KEYP and *VALUEP to point to storage managed by the getrcskey
 * function; the contents are only valid until the next call to
 * getrcskey or getrcsrev.  If LENP is not NULL, this sets *LENP to
 * the length of *VALUEP; this is needed if the string might contain
 * binary data.
 */

static char *key = NULL;
static char *value = NULL;
static size_t keysize = 0;
static size_t valsize = 0;

static int
getrcskey (fp, keyp, valp, lenp)
    FILE *fp;
    char **keyp;
    char **valp;
    size_t *lenp;
{
    char *cur, *max;
    int c;
    int just_string;

    if (lenp != NULL)
        *lenp = 0;

    /* skip leading whitespace */
    do
    {
	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
    } while (whitespace (c));

    /* fill in key */
    cur = key;
    max = key + keysize;
    while (!whitespace (c) && c != ';')
    {
	if (cur >= max)
	{
	    size_t curoff = cur - key;
	    expand_string (&key, &keysize, keysize + 1);
	    cur = key + curoff;
	    max = key + keysize;
	}
	*cur++ = c;

	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
    }
    if (cur >= max)
    {
	size_t curoff = cur - key;
	expand_string (&key, &keysize, keysize + 1);
	cur = key + curoff;
	max = key + keysize;
    }
    *cur = '\0';

    /* skip whitespace between key and val */
    while (whitespace (c))
    {
	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
    } 

    /* if we ended key with a semicolon, there is no value */
    if (c == ';')
    {
	*keyp = key;
	*valp = (char *) NULL;
	return (0);
    }

    /* otherwise, there might be a value, so fill it in */
    cur = value;
    max = value + valsize;

    just_string = (strcmp (key, RCSDESC) == 0
		   || strcmp (key, "text") == 0
		   || strcmp (key, "log") == 0);

    /* process the value */
    for (;;)
    {
	/* handle RCS "strings" */
	if (c == '@') 
	{
	    for (;;)
	    {
		c = getc (fp);
		if (c == EOF)
		{
		    *keyp = (char *) NULL;
		    *valp = (char *) NULL;
		    return (-1);
		}

		if (c == '@')
		{
		    c = getc (fp);
		    if (c == EOF)
		    {
			*keyp = (char *) NULL;
			*valp = (char *) NULL;
			return (-1);
		    }
		    
		    if (c != '@')
			break;
		}

		if (cur >= max)
		{
		    size_t curoff = cur - value;
		    expand_string (&value, &valsize, valsize + 1);
		    cur = value + curoff;
		    max = value + valsize;
		}
		*cur++ = c;
	    }
	}

	/* The syntax for some key-value pairs is different; they
	   don't end with a semicolon.  */
	if (just_string)
	    break;

	/* compress whitespace down to a single space */
	if (whitespace (c))
	{
	    do {
		c = getc (fp);
		if (c == EOF)
		{
		    *keyp = (char *) NULL;
		    *valp = (char *) NULL;
		    return (-1);
		}
	    } while (whitespace (c));

	    /* Do not include any trailing whitespace in the value. */
	    if (c != ';')
	    {
		if (cur >= max)
		{
		    size_t curoff = cur - value;
		    expand_string (&value, &valsize, valsize + 1);
		    cur = value + curoff;
		    max = value + valsize;
		}
		*cur++ = ' ';
	    }
	}

	/* if we got a semi-colon we are done with the entire value */
	if (c == ';')
	    break;

	if (cur >= max)
	{
	    size_t curoff = cur - value;
	    expand_string (&value, &valsize, valsize + 1);
	    cur = value + curoff;
	    max = value + valsize;
	}
	*cur++ = c;

	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
    }

    /* terminate the string */
    if (cur >= max)
    {
	size_t curoff = cur - value;
	expand_string (&value, &valsize, valsize + 1);
	cur = value + curoff;
	max = value + valsize;
    }
    *cur = '\0';

    /* if the string is empty, make it null */
    if (value && cur != value)
    {
	*valp = value;
	if (lenp != NULL)
	    *lenp = cur - value;
    }
    else
	*valp = NULL;
    *keyp = key;
    return (0);
}

/* Read an RCS revision number from FP.  Put a pointer to it in *REVP;
   it points to space managed by getrcsrev which is only good until
   the next call to getrcskey or getrcsrev.  */
static void
getrcsrev (fp, revp)
    FILE *fp;
    char **revp;
{
    char *cur;
    char *max;
    int c;

    do {
	c = getc (fp);
	if (c == EOF)
	{
	    /* FIXME: should be including filename in error message.  */
	    if (ferror (fp))
		error (1, errno, "cannot read rcs file");
	    else
		error (1, 0, "unexpected end of file reading rcs file");
	}
    } while (whitespace (c));

    if (!(isdigit (c) || c == '.'))
	/* FIXME: should be including filename in error message.  */
	error (1, 0, "error reading rcs file; revision number expected");

    cur = key;
    max = key + keysize;
    while (isdigit (c) || c == '.')
    {
	if (cur >= max)
	{
	    size_t curoff = cur - key;
	    expand_string (&key, &keysize, keysize + 1);
	    cur = key + curoff;
	    max = key + keysize;
	}
	*cur++ = c;

	c = getc (fp);
	if (c == EOF)
	{
	    /* FIXME: should be including filename in error message.  */
	    if (ferror (fp))
		error (1, errno, "cannot read rcs file");
	    else
		error (1, 0, "unexpected end of file reading rcs file");
	}
    }

    if (cur >= max)
    {
	size_t curoff = cur - key;
	expand_string (&key, &keysize, keysize + 1);
	cur = key + curoff;
	max = key + keysize;
    }
    *cur = '\0';
    *revp = key;
}

/* Like getrcsrev, but don't die on error.  Return the last character
   read (last call to getc, which may be EOF).  TODO: implement getrcsrev
   in terms of this function. */
static int
getrevnum (fp, revp)
    FILE *fp;
    char **revp;
{
    char *cur;
    char *max;
    int c;

    *revp = NULL;
    do {
	c = getc (fp);
	if (c == EOF)
	    return c;
    } while (whitespace (c));

    if (!(isdigit (c) || c == '.'))
	return c;

    cur = key;
    max = key + keysize;
    while (isdigit (c) || c == '.')
    {
	if (cur >= max)
	{
	    size_t curoff = cur - key;
	    expand_string (&key, &keysize, keysize + 1);
	    cur = key + curoff;
	    max = key + keysize;
	}
	*cur = c;

	c = getc (fp);
	if (c == EOF)
	    break;
	cur++;
    }

    if (cur >= max)
    {
	size_t curoff = cur - key;
	expand_string (&key, &keysize, keysize + 1);
	cur = key + curoff;
	max = key + keysize;
    }
    *cur = '\0';
    *revp = key;
    return c;
}

/*
 * process the symbols list of the rcs file
 */
static void
do_symbols (list, val)
    List *list;
    char *val;
{
    Node *p;
    char *cp = val;
    char *tag, *rev;

    for (;;)
    {
	/* skip leading whitespace */
	while (whitespace (*cp))
	    cp++;

	/* if we got to the end, we are done */
	if (*cp == '\0')
	    break;

	/* split it up into tag and rev */
	tag = cp;
	cp = strchr (cp, ':');
	*cp++ = '\0';
	rev = cp;
	while (!whitespace (*cp) && *cp != '\0')
	    cp++;
	if (*cp != '\0')
	    *cp++ = '\0';

	/* make a new node and add it to the list */
	p = getnode ();
	p->key = xstrdup (tag);
	p->data = xstrdup (rev);
	(void) addnode (list, p);
    }
}

/*
 * process the locks list of the rcs file
 * Like do_symbols, but hash entries are keyed backwards: i.e.
 * an entry like `user:rev' is keyed on REV rather than on USER.
 */
static void
do_locks (list, val)
    List *list;
    char *val;
{
    Node *p;
    char *cp = val;
    char *user, *rev;

    for (;;)
    {
	/* skip leading whitespace */
	while (whitespace (*cp))
	    cp++;

	/* if we got to the end, we are done */
	if (*cp == '\0')
	    break;

	/* split it up into user and rev */
	user = cp;
	cp = strchr (cp, ':');
	*cp++ = '\0';
	rev = cp;
	while (!whitespace (*cp) && *cp != '\0')
	    cp++;
	if (*cp != '\0')
	    *cp++ = '\0';

	/* make a new node and add it to the list */
	p = getnode ();
	p->key = xstrdup (rev);
	p->data = xstrdup (user);
	(void) addnode (list, p);
    }
}

/*
 * process the branches list of a revision delta
 */
static void
do_branches (list, val)
    List *list;
    char *val;
{
    Node *p;
    char *cp = val;
    char *branch;

    for (;;)
    {
	/* skip leading whitespace */
	while (whitespace (*cp))
	    cp++;

	/* if we got to the end, we are done */
	if (*cp == '\0')
	    break;

	/* find the end of this branch */
	branch = cp;
	while (!whitespace (*cp) && *cp != '\0')
	    cp++;
	if (*cp != '\0')
	    *cp++ = '\0';

	/* make a new node and add it to the list */
	p = getnode ();
	p->key = xstrdup (branch);
	(void) addnode (list, p);
    }
}

/*
 * Version Number
 * 
 * Returns the requested version number of the RCS file, satisfying tags and/or
 * dates, and walking branches, if necessary.
 * 
 * The result is returned; null-string if error.
 */
char *
RCS_getversion (rcs, tag, date, force_tag_match, simple_tag)
    RCSNode *rcs;
    char *tag;
    char *date;
    int force_tag_match;
    int *simple_tag;
{
    if (simple_tag != NULL)
	*simple_tag = 0;

    /* make sure we have something to look at... */
    assert (rcs != NULL);

    if (tag && date)
    {
	char *branch, *rev;

	if (! RCS_nodeisbranch (rcs, tag))
	{
	    /* We can't get a particular date if the tag is not a
               branch.  */
	    return NULL;
	}

	/* Work out the branch.  */
	if (! isdigit (tag[0]))
	    branch = RCS_whatbranch (rcs, tag);
	else
	    branch = xstrdup (tag);

	/* Fetch the revision of branch as of date.  */
	rev = RCS_getdatebranch (rcs, date, branch);
	free (branch);
	return (rev);
    }
    else if (tag)
	return (RCS_gettag (rcs, tag, force_tag_match, simple_tag));
    else if (date)
	return (RCS_getdate (rcs, date, force_tag_match));
    else
	return (RCS_head (rcs));

}

/*
 * Find the revision for a specific tag.
 * If force_tag_match is set, return NULL if an exact match is not
 * possible otherwise return RCS_head ().  We are careful to look for
 * and handle "magic" revisions specially.
 * 
 * If the matched tag is a branch tag, find the head of the branch.
 * 
 * Returns pointer to newly malloc'd string, or NULL.
 */
char *
RCS_gettag (rcs, symtag, force_tag_match, simple_tag)
    RCSNode *rcs;
    char *symtag;
    int force_tag_match;
    int *simple_tag;
{
    char *tag = symtag;
    int tag_allocated = 0;

    if (simple_tag != NULL)
	*simple_tag = 0;

    /* make sure we have something to look at... */
    assert (rcs != NULL);

    /* XXX this is probably not necessary, --jtc */
    if (rcs->flags & PARTIAL) 
	RCS_reparsercsfile (rcs, NULL);

    /* If tag is "HEAD", special case to get head RCS revision */
    if (tag && (strcmp (tag, TAG_HEAD) == 0 || *tag == '\0'))
#if 0 /* This #if 0 is only in the Cygnus code.  Why?  Death support?  */
	if (force_tag_match && (rcs->flags & VALID) && (rcs->flags & INATTIC))
	    return ((char *) NULL);	/* head request for removed file */
	else
#endif
	    return (RCS_head (rcs));

    if (!isdigit (tag[0]))
    {
	char *version;

	/* If we got a symbolic tag, resolve it to a numeric */
	version = translate_symtag (rcs, tag);
	if (version != NULL)
	{
	    int dots;
	    char *magic, *branch, *cp;

	    tag = version;
	    tag_allocated = 1;

	    /*
	     * If this is a magic revision, we turn it into either its
	     * physical branch equivalent (if one exists) or into
	     * its base revision, which we assume exists.
	     */
	    dots = numdots (tag);
	    if (dots > 2 && (dots & 1) != 0)
	    {
		branch = strrchr (tag, '.');
		cp = branch++ - 1;
		while (*cp != '.')
		    cp--;

		/* see if we have .magic-branch. (".0.") */
		magic = xmalloc (strlen (tag) + 1);
		(void) sprintf (magic, ".%d.", RCS_MAGIC_BRANCH);
		if (strncmp (magic, cp, strlen (magic)) == 0)
		{
		    /* it's magic.  See if the branch exists */
		    *cp = '\0';		/* turn it into a revision */
		    (void) sprintf (magic, "%s.%s", tag, branch);
		    branch = RCS_getbranch (rcs, magic, 1);
		    free (magic);
		    if (branch != NULL)
		    {
			free (tag);
			return (branch);
		    }
		    return (tag);
		}
		free (magic);
	    }
	}
	else
	{
	    /* The tag wasn't there, so return the head or NULL */
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
    }

    /*
     * numeric tag processing:
     *		1) revision number - just return it
     *		2) branch number   - find head of branch
     */

    /* strip trailing dots */
    while (tag[strlen (tag) - 1] == '.')
	tag[strlen (tag) - 1] = '\0';

    if ((numdots (tag) & 1) == 0)
    {
	char *branch;

	/* we have a branch tag, so we need to walk the branch */
	branch = RCS_getbranch (rcs, tag, force_tag_match);
	if (tag_allocated)
	    free (tag);
	return branch;
    }
    else
    {
	Node *p;

	/* we have a revision tag, so make sure it exists */
	p = findnode (rcs->versions, tag);
	if (p != NULL)
	{
	    /* We have found a numeric revision for the revision tag.
	       To support expanding the RCS keyword Name, if
	       SIMPLE_TAG is not NULL, tell the the caller that this
	       is a simple tag which co will recognize.  FIXME: Are
	       there other cases in which we should set this?  In
	       particular, what if we expand RCS keywords internally
	       without calling co?  */
	    if (simple_tag != NULL)
		*simple_tag = 1;
	    if (! tag_allocated)
		tag = xstrdup (tag);
	    return (tag);
	}
	else
	{
	    /* The revision wasn't there, so return the head or NULL */
	    if (tag_allocated)
		free (tag);
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
    }
}

/*
 * Return a "magic" revision as a virtual branch off of REV for the RCS file.
 * A "magic" revision is one which is unique in the RCS file.  By unique, I
 * mean we return a revision which:
 *	- has a branch of 0 (see rcs.h RCS_MAGIC_BRANCH)
 *	- has a revision component which is not an existing branch off REV
 *	- has a revision component which is not an existing magic revision
 *	- is an even-numbered revision, to avoid conflicts with vendor branches
 * The first point is what makes it "magic".
 *
 * As an example, if we pass in 1.37 as REV, we will look for an existing
 * branch called 1.37.2.  If it did not exist, we would look for an
 * existing symbolic tag with a numeric part equal to 1.37.0.2.  If that
 * didn't exist, then we know that the 1.37.2 branch can be reserved by
 * creating a symbolic tag with 1.37.0.2 as the numeric part.
 *
 * This allows us to fork development with very little overhead -- just a
 * symbolic tag is used in the RCS file.  When a commit is done, a physical
 * branch is dynamically created to hold the new revision.
 *
 * Note: We assume that REV is an RCS revision and not a branch number.
 */
static char *check_rev;
char *
RCS_magicrev (rcs, rev)
    RCSNode *rcs;
    char *rev;
{
    int rev_num;
    char *xrev, *test_branch;

    xrev = xmalloc (strlen (rev) + 14); /* enough for .0.number */
    check_rev = xrev;

    /* only look at even numbered branches */
    for (rev_num = 2; ; rev_num += 2)
    {
	/* see if the physical branch exists */
	(void) sprintf (xrev, "%s.%d", rev, rev_num);
	test_branch = RCS_getbranch (rcs, xrev, 1);
	if (test_branch != NULL)	/* it did, so keep looking */
	{
	    free (test_branch);
	    continue;
	}

	/* now, create a "magic" revision */
	(void) sprintf (xrev, "%s.%d.%d", rev, RCS_MAGIC_BRANCH, rev_num);

	/* walk the symbols list to see if a magic one already exists */
	if (walklist (RCS_symbols(rcs), checkmagic_proc, NULL) != 0)
	    continue;

	/* we found a free magic branch.  Claim it as ours */
	return (xrev);
    }
}

/*
 * walklist proc to look for a match in the symbols list.
 * Returns 0 if the symbol does not match, 1 if it does.
 */
static int
checkmagic_proc (p, closure)
    Node *p;
    void *closure;
{
    if (strcmp (check_rev, p->data) == 0)
	return (1);
    else
	return (0);
}

/*
 * Given an RCSNode, returns non-zero if the specified revision number 
 * or symbolic tag resolves to a "branch" within the rcs file.
 *
 * FIXME: this is the same as RCS_nodeisbranch except for the special 
 *        case for handling a null rcsnode.
 */
int
RCS_isbranch (rcs, rev)
    RCSNode *rcs;
    const char *rev;
{
    /* numeric revisions are easy -- even number of dots is a branch */
    if (isdigit (*rev))
	return ((numdots (rev) & 1) == 0);

    /* assume a revision if you can't find the RCS info */
    if (rcs == NULL)
	return (0);

    /* now, look for a match in the symbols list */
    return (RCS_nodeisbranch (rcs, rev));
}

/*
 * Given an RCSNode, returns non-zero if the specified revision number
 * or symbolic tag resolves to a "branch" within the rcs file.  We do
 * take into account any magic branches as well.
 */
int
RCS_nodeisbranch (rcs, rev)
    RCSNode *rcs;
    const char *rev;
{
    int dots;
    char *version;

    assert (rcs != NULL);

    /* numeric revisions are easy -- even number of dots is a branch */
    if (isdigit (*rev))
	return ((numdots (rev) & 1) == 0);

    version = translate_symtag (rcs, rev);
    if (version == NULL)
	return (0);
    dots = numdots (version);
    if ((dots & 1) == 0)
    {
	free (version);
	return (1);
    }

    /* got a symbolic tag match, but it's not a branch; see if it's magic */
    if (dots > 2)
    {
	char *magic;
	char *branch = strrchr (version, '.');
	char *cp = branch - 1;
	while (*cp != '.')
	    cp--;

	/* see if we have .magic-branch. (".0.") */
	magic = xmalloc (strlen (version) + 1);
	(void) sprintf (magic, ".%d.", RCS_MAGIC_BRANCH);
	if (strncmp (magic, cp, strlen (magic)) == 0)
	{
	    free (magic);
	    free (version);
	    return (1);
	}
	free (magic);
	free (version);
    }
    return (0);
}

/*
 * Returns a pointer to malloc'ed memory which contains the branch
 * for the specified *symbolic* tag.  Magic branches are handled correctly.
 */
char *
RCS_whatbranch (rcs, rev)
    RCSNode *rcs;
    const char *rev;
{
    char *version;
    int dots;

    /* assume no branch if you can't find the RCS info */
    if (rcs == NULL)
	return ((char *) NULL);

    /* now, look for a match in the symbols list */
    version = translate_symtag (rcs, rev);
    if (version == NULL)
	return ((char *) NULL);
    dots = numdots (version);
    if ((dots & 1) == 0)
	return (version);

    /* got a symbolic tag match, but it's not a branch; see if it's magic */
    if (dots > 2)
    {
	char *magic;
	char *branch = strrchr (version, '.');
	char *cp = branch++ - 1;
	while (*cp != '.')
	    cp--;

	/* see if we have .magic-branch. (".0.") */
	magic = xmalloc (strlen (version) + 1);
	(void) sprintf (magic, ".%d.", RCS_MAGIC_BRANCH);
	if (strncmp (magic, cp, strlen (magic)) == 0)
	{
	    /* yep.  it's magic.  now, construct the real branch */
	    *cp = '\0';			/* turn it into a revision */
	    (void) sprintf (magic, "%s.%s", version, branch);
	    free (version);
	    return (magic);
	}
	free (magic);
	free (version);
    }
    return ((char *) NULL);
}

/*
 * Get the head of the specified branch.  If the branch does not exist,
 * return NULL or RCS_head depending on force_tag_match.
 * Returns NULL or a newly malloc'd string.
 */
char *
RCS_getbranch (rcs, tag, force_tag_match)
    RCSNode *rcs;
    char *tag;
    int force_tag_match;
{
    Node *p, *head;
    RCSVers *vn;
    char *xtag;
    char *nextvers;
    char *cp;

    /* make sure we have something to look at... */
    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    /* find out if the tag contains a dot, or is on the trunk */
    cp = strrchr (tag, '.');

    /* trunk processing is the special case */
    if (cp == NULL)
    {
	xtag = xmalloc (strlen (tag) + 1 + 1);	/* +1 for an extra . */
	(void) strcpy (xtag, tag);
	(void) strcat (xtag, ".");
	for (cp = rcs->head; cp != NULL;)
	{
	    if (strncmp (xtag, cp, strlen (xtag)) == 0)
		break;
	    p = findnode (rcs->versions, cp);
	    if (p == NULL)
	    {
		free (xtag);
		if (force_tag_match)
		    return (NULL);
		else
		    return (RCS_head (rcs));
	    }
	    vn = (RCSVers *) p->data;
	    cp = vn->next;
	}
	free (xtag);
	if (cp == NULL)
	{
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
	return (xstrdup (cp));
    }

    /* if it had a `.', terminate the string so we have the base revision */
    *cp = '\0';

    /* look up the revision this branch is based on */
    p = findnode (rcs->versions, tag);

    /* put the . back so we have the branch again */
    *cp = '.';

    if (p == NULL)
    {
	/* if the base revision didn't exist, return head or NULL */
	if (force_tag_match)
	    return (NULL);
	else
	    return (RCS_head (rcs));
    }

    /* find the first element of the branch we are looking for */
    vn = (RCSVers *) p->data;
    if (vn->branches == NULL)
	return (NULL);
    xtag = xmalloc (strlen (tag) + 1 + 1);	/* 1 for the extra '.' */
    (void) strcpy (xtag, tag);
    (void) strcat (xtag, ".");
    head = vn->branches->list;
    for (p = head->next; p != head; p = p->next)
	if (strncmp (p->key, xtag, strlen (xtag)) == 0)
	    break;
    free (xtag);

    if (p == head)
    {
	/* we didn't find a match so return head or NULL */
	if (force_tag_match)
	    return (NULL);
	else
	    return (RCS_head (rcs));
    }

    /* now walk the next pointers of the branch */
    nextvers = p->key;
    do
    {
	p = findnode (rcs->versions, nextvers);
	if (p == NULL)
	{
	    /* a link in the chain is missing - return head or NULL */
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
	vn = (RCSVers *) p->data;
	nextvers = vn->next;
    } while (nextvers != NULL);

    /* we have the version in our hand, so go for it */
    return (xstrdup (vn->version));
}

/* Get the branch point for a particular branch, that is the first
   revision on that branch.  For example, RCS_getbranchpoint (rcs,
   "1.3.2") will normally return "1.3.2.1".  TARGET may be either a
   branch number or a revision number; if a revnum, find the
   branchpoint of the branch to which TARGET belongs.

   Return RCS_head if TARGET is on the trunk or if the root node could
   not be found (this is sort of backwards from our behavior on a branch;
   the rationale is that the return value is a revision from which you
   can start walking the next fields and end up at TARGET).
   Return NULL on error.  */

static char *
RCS_getbranchpoint (rcs, target)
    RCSNode *rcs;
    char *target;
{
    char *branch, *bp;
    Node *vp;
    RCSVers *rev;
    int dots, isrevnum, brlen;

    dots = numdots (target);
    isrevnum = dots & 1;

    if (dots == 1)
	/* TARGET is a trunk revision; return rcs->head. */
	return (RCS_head (rcs));

    /* Get the revision number of the node at which TARGET's branch is
       rooted.  If TARGET is a branch number, lop off the last field;
       if it's a revision number, lop off the last *two* fields. */
    branch = xstrdup (target);
    bp = strrchr (branch, '.');
    if (bp == NULL)
	error (1, 0, "%s: confused revision number %s",
	       rcs->path, target);
    if (isrevnum)
	while (*--bp != '.')
	    ;
    *bp = '\0';

    vp = findnode (rcs->versions, branch);
    if (vp == NULL)
    {	
	error (0, 0, "%s: can't find branch point %s", rcs->path, target);
	return NULL;
    }
    rev = (RCSVers *) vp->data;

    *bp++ = '.';
    while (*bp && *bp != '.')
	++bp;
    brlen = bp - branch;

    vp = rev->branches->list->next;
    while (vp != rev->branches->list)
    {
	/* BRANCH may be a genuine branch number, e.g. `1.1.3', or
	   maybe a full revision number, e.g. `1.1.3.6'.  We have
	   found our branch point if the first BRANCHLEN characters
	   of the revision number match, *and* if the following
	   character is a dot. */
	if (strncmp (vp->key, branch, brlen) == 0 && vp->key[brlen] == '.')
	    break;
	vp = vp->next;
    }

    free (branch);
    if (vp == rev->branches->list)
    {
	error (0, 0, "%s: can't find branch point %s", rcs->path, target);
	return NULL;
    }
    else
	return (xstrdup (vp->key));
}

/*
 * Get the head of the RCS file.  If branch is set, this is the head of the
 * branch, otherwise the real head.
 * Returns NULL or a newly malloc'd string.
 */
char *
RCS_head (rcs)
    RCSNode *rcs;
{
    /* make sure we have something to look at... */
    assert (rcs != NULL);

    /*
     * NOTE: we call getbranch with force_tag_match set to avoid any
     * possibility of recursion
     */
    if (rcs->branch)
	return (RCS_getbranch (rcs, rcs->branch, 1));
    else
	return (xstrdup (rcs->head));
}

/*
 * Get the most recent revision, based on the supplied date, but use some
 * funky stuff and follow the vendor branch maybe
 */
char *
RCS_getdate (rcs, date, force_tag_match)
    RCSNode *rcs;
    char *date;
    int force_tag_match;
{
    char *cur_rev = NULL;
    char *retval = NULL;
    Node *p;
    RCSVers *vers = NULL;

    /* make sure we have something to look at... */
    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    /* if the head is on a branch, try the branch first */
    if (rcs->branch != NULL)
	retval = RCS_getdatebranch (rcs, date, rcs->branch);

    /* if we found a match, we are done */
    if (retval != NULL)
	return (retval);

    /* otherwise if we have a trunk, try it */
    if (rcs->head)
    {
	p = findnode (rcs->versions, rcs->head);
	while (p != NULL)
	{
	    /* if the date of this one is before date, take it */
	    vers = (RCSVers *) p->data;
	    if (RCS_datecmp (vers->date, date) <= 0)
	    {
		cur_rev = vers->version;
		break;
	    }

	    /* if there is a next version, find the node */
	    if (vers->next != NULL)
		p = findnode (rcs->versions, vers->next);
	    else
		p = (Node *) NULL;
	}
    }

    /*
     * at this point, either we have the revision we want, or we have the
     * first revision on the trunk (1.1?) in our hands
     */

    /* if we found what we're looking for, and it's not 1.1 return it */
    if (cur_rev != NULL && strcmp (cur_rev, "1.1") != 0)
	return (xstrdup (cur_rev));

    /* look on the vendor branch */
    retval = RCS_getdatebranch (rcs, date, CVSBRANCH);

    /*
     * if we found a match, return it; otherwise, we return the first
     * revision on the trunk or NULL depending on force_tag_match and the
     * date of the first rev
     */
    if (retval != NULL)
	return (retval);

    if (!force_tag_match || RCS_datecmp (vers->date, date) <= 0)
	return (xstrdup (vers->version));
    else
	return (NULL);
}

/*
 * Look up the last element on a branch that was put in before the specified
 * date (return the rev or NULL)
 */
static char *
RCS_getdatebranch (rcs, date, branch)
    RCSNode *rcs;
    char *date;
    char *branch;
{
    char *cur_rev = NULL;
    char *cp;
    char *xbranch, *xrev;
    Node *p;
    RCSVers *vers;

    /* look up the first revision on the branch */
    xrev = xstrdup (branch);
    cp = strrchr (xrev, '.');
    if (cp == NULL)
    {
	free (xrev);
	return (NULL);
    }
    *cp = '\0';				/* turn it into a revision */

    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    p = findnode (rcs->versions, xrev);
    free (xrev);
    if (p == NULL)
	return (NULL);
    vers = (RCSVers *) p->data;

    /* Tentatively use this revision, if it is early enough.  */
    if (RCS_datecmp (vers->date, date) <= 0)
	cur_rev = vers->version;

    /* If no branches list, return now.  This is what happens if the branch
       is a (magic) branch with no revisions yet.  */
    if (vers->branches == NULL)
	return xstrdup (cur_rev);

    /* walk the branches list looking for the branch number */
    xbranch = xmalloc (strlen (branch) + 1 + 1); /* +1 for the extra dot */
    (void) strcpy (xbranch, branch);
    (void) strcat (xbranch, ".");
    for (p = vers->branches->list->next; p != vers->branches->list; p = p->next)
	if (strncmp (p->key, xbranch, strlen (xbranch)) == 0)
	    break;
    free (xbranch);
    if (p == vers->branches->list)
    {
	/* This is what happens if the branch is a (magic) branch with
	   no revisions yet.  Similar to the case where vers->branches ==
	   NULL, except here there was a another branch off the same
	   branchpoint.  */
	return xstrdup (cur_rev);
    }

    p = findnode (rcs->versions, p->key);

    /* walk the next pointers until you find the end, or the date is too late */
    while (p != NULL)
    {
	vers = (RCSVers *) p->data;
	if (RCS_datecmp (vers->date, date) <= 0)
	    cur_rev = vers->version;
	else
	    break;

	/* if there is a next version, find the node */
	if (vers->next != NULL)
	    p = findnode (rcs->versions, vers->next);
	else
	    p = (Node *) NULL;
    }

    /* Return whatever we found, which may be NULL.  */
    return xstrdup (cur_rev);
}

/*
 * Compare two dates in RCS format. Beware the change in format on January 1,
 * 2000, when years go from 2-digit to full format.
 */
int
RCS_datecmp (date1, date2)
    char *date1, *date2;
{
    int length_diff = strlen (date1) - strlen (date2);

    return (length_diff ? length_diff : strcmp (date1, date2));
}

/* Look up revision REV in RCS and return the date specified for the
   revision minus FUDGE seconds (FUDGE will generally be one, so that the
   logically previous revision will be found later, or zero, if we want
   the exact date).

   The return value is the date being returned as a time_t, or (time_t)-1
   on error (previously was documented as zero on error; I haven't checked
   the callers to make sure that they really check for (time_t)-1, but
   the latter is what this function really returns).  If DATE is non-NULL,
   then it must point to MAXDATELEN characters, and we store the same
   return value there in DATEFORM format.  */
time_t
RCS_getrevtime (rcs, rev, date, fudge)
    RCSNode *rcs;
    char *rev;
    char *date;
    int fudge;
{
    char tdate[MAXDATELEN];
    struct tm xtm, *ftm;
    time_t revdate = 0;
    Node *p;
    RCSVers *vers;

    /* make sure we have something to look at... */
    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    /* look up the revision */
    p = findnode (rcs->versions, rev);
    if (p == NULL)
	return (-1);
    vers = (RCSVers *) p->data;

    /* split up the date */
    ftm = &xtm;
    (void) sscanf (vers->date, SDATEFORM, &ftm->tm_year, &ftm->tm_mon,
		   &ftm->tm_mday, &ftm->tm_hour, &ftm->tm_min,
		   &ftm->tm_sec);

    /* If the year is from 1900 to 1999, RCS files contain only two
       digits, and sscanf gives us a year from 0-99.  If the year is
       2000+, RCS files contain all four digits and we subtract 1900,
       because the tm_year field should contain years since 1900.  */

    if (ftm->tm_year > 1900)
	ftm->tm_year -= 1900;

    /* put the date in a form getdate can grok */
    (void) sprintf (tdate, "%d/%d/%d GMT %d:%d:%d", ftm->tm_mon,
		    ftm->tm_mday, ftm->tm_year + 1900, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);

    /* turn it into seconds since the epoch */
    revdate = get_date (tdate, (struct timeb *) NULL);
    if (revdate != (time_t) -1)
    {
	revdate -= fudge;		/* remove "fudge" seconds */
	if (date)
	{
	    /* put an appropriate string into ``date'' if we were given one */
	    ftm = gmtime (&revdate);
	    (void) sprintf (date, DATEFORM,
			    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
			    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
			    ftm->tm_min, ftm->tm_sec);
	}
    }
    return (revdate);
}

List *
RCS_getlocks (rcs)
    RCSNode *rcs;
{
    assert(rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    if (rcs->locks_data) {
	rcs->locks = getlist ();
	do_locks (rcs->locks, rcs->locks_data);
	free(rcs->locks_data);
	rcs->locks_data = NULL;
    }

    return rcs->locks;
}

List *
RCS_symbols(rcs)
    RCSNode *rcs;
{
    assert(rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    if (rcs->symbols_data) {
	rcs->symbols = getlist ();
	do_symbols (rcs->symbols, rcs->symbols_data);
	free(rcs->symbols_data);
	rcs->symbols_data = NULL;
    }

    return rcs->symbols;
}

/*
 * Return the version associated with a particular symbolic tag.
 * Returns NULL or a newly malloc'd string.
 */
static char *
translate_symtag (rcs, tag)
    RCSNode *rcs;
    const char *tag;
{
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    if (rcs->symbols != NULL)
    {
	Node *p;

	/* The symbols have already been converted into a list.  */
	p = findnode (rcs->symbols, tag);
	if (p == NULL)
	    return NULL;

	return xstrdup (p->data);
    }

    if (rcs->symbols_data != NULL)
    {
	size_t len;
	char *cp;

	/* Look through the RCS symbols information.  This is like
           do_symbols, but we don't add the information to a list.  In
           most cases, we will only be called once for this file, so
           generating the list is unnecessary overhead.  */

	len = strlen (tag);
	cp = rcs->symbols_data;
	while ((cp = strchr (cp, tag[0])) != NULL)
	{
	    if ((cp == rcs->symbols_data || whitespace (cp[-1]))
		&& strncmp (cp, tag, len) == 0
		&& cp[len] == ':')
	    {
		char *v, *r;

		/* We found the tag.  Return the version number.  */

		cp += len + 1;
		v = cp;
		while (! whitespace (*cp) && *cp != '\0')
		    ++cp;
		r = xmalloc (cp - v + 1);
		strncpy (r, v, cp - v);
		r[cp - v] = '\0';
		return r;
	    }

	    while (! whitespace (*cp) && *cp != '\0')
		++cp;
	}
    }

    return NULL;
}

/*
 * The argument ARG is the getopt remainder of the -k option specified on the
 * command line.  This function returns malloc'ed space that can be used
 * directly in calls to RCS V5, with the -k flag munged correctly.
 */
char *
RCS_check_kflag (arg)
    const char *arg;
{
    static const char *const  keyword_usage[] =
    {
      "%s %s: invalid RCS keyword expansion mode\n",
      "Valid expansion modes include:\n",
      "   -kkv\tGenerate keywords using the default form.\n",
      "   -kkvl\tLike -kkv, except locker's name inserted.\n",
      "   -kk\tGenerate only keyword names in keyword strings.\n",
      "   -kv\tGenerate only keyword values in keyword strings.\n",
      "   -ko\tGenerate the old keyword string (no changes from checked in file).\n",
      "   -kb\tGenerate binary file unmodified (merges not allowed) (RCS 5.7).\n",
      "(Specify the --help global option for a list of other help options)\n",
      NULL,
    };
    /* Big enough to hold any of the strings from kflags.  */
    char karg[10];
    char const *const *cpp = NULL;

    if (arg)
    {
	for (cpp = kflags; *cpp != NULL; cpp++)
	{
	    if (strcmp (arg, *cpp) == 0)
		break;
	}
    }

    if (arg == NULL || *cpp == NULL)
    {
	usage (keyword_usage);
    }

    (void) sprintf (karg, "-k%s", *cpp);
    return (xstrdup (karg));
}

/*
 * Do some consistency checks on the symbolic tag... These should equate
 * pretty close to what RCS checks, though I don't know for certain.
 */
void
RCS_check_tag (tag)
    const char *tag;
{
    char *invalid = "$,.:;@";		/* invalid RCS tag characters */
    const char *cp;

    /*
     * The first character must be an alphabetic letter. The remaining
     * characters cannot be non-visible graphic characters, and must not be
     * in the set of "invalid" RCS identifier characters.
     */
    if (isalpha (*tag))
    {
	for (cp = tag; *cp; cp++)
	{
	    if (!isgraph (*cp))
		error (1, 0, "tag `%s' has non-visible graphic characters",
		       tag);
	    if (strchr (invalid, *cp))
		error (1, 0, "tag `%s' must not contain the characters `%s'",
		       tag, invalid);
	}
    }
    else
	error (1, 0, "tag `%s' must start with a letter", tag);
}

/*
 * Return true if RCS revision with TAG is a dead revision.
 */
int
RCS_isdead (rcs, tag)
    RCSNode *rcs;
    const char *tag;
{
    Node *p;
    RCSVers *version;

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    p = findnode (rcs->versions, tag);
    if (p == NULL)
	return (0);

    version = (RCSVers *) p->data;
    return (version->dead);
}

/* Return the RCS keyword expansion mode.  For example "b" for binary.
   Returns a pointer into storage which is allocated and freed along with
   the rest of the RCS information; the caller should not modify this
   storage.  Returns NULL if the RCS file does not specify a keyword
   expansion mode; for all other errors, die with a fatal error.  */
char *
RCS_getexpand (rcs)
    RCSNode *rcs;
{
    assert (rcs != NULL);
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);
    return rcs->expand;
}

/* RCS keywords, and a matching enum.  */
struct rcs_keyword
{
    const char *string;
    size_t len;
};
#define KEYWORD_INIT(s) (s), sizeof (s) - 1
static const struct rcs_keyword keywords[] =
{
    { KEYWORD_INIT ("Author") },
    { KEYWORD_INIT ("Date") },
    { KEYWORD_INIT ("Header") },
    { KEYWORD_INIT ("Id") },
    { KEYWORD_INIT ("Locker") },
    { KEYWORD_INIT ("Log") },
    { KEYWORD_INIT ("Name") },
    { KEYWORD_INIT ("RCSfile") },
    { KEYWORD_INIT ("Revision") },
    { KEYWORD_INIT ("Source") },
    { KEYWORD_INIT ("State") },
    { NULL, 0 }
};
enum keyword
{
    KEYWORD_AUTHOR = 0,
    KEYWORD_DATE,
    KEYWORD_HEADER,
    KEYWORD_ID,
    KEYWORD_LOCKER,
    KEYWORD_LOG,
    KEYWORD_NAME,
    KEYWORD_RCSFILE,
    KEYWORD_REVISION,
    KEYWORD_SOURCE,
    KEYWORD_STATE
};

/* Convert an RCS date string into a readable string.  This is like
   the RCS date2str function.  */

static char *
printable_date (rcs_date)
     const char *rcs_date;
{
    int year, mon, mday, hour, min, sec;
    char buf[100];

    (void) sscanf (rcs_date, SDATEFORM, &year, &mon, &mday, &hour, &min,
		   &sec);
    if (year < 1900)
	year += 1900;
    sprintf (buf, "%04d/%02d/%02d %02d:%02d:%02d", year, mon, mday,
	     hour, min, sec);
    return xstrdup (buf);
}

/* Escape the characters in a string so that it can be included in an
   RCS value.  */

static char *
escape_keyword_value (value, free_value)
     const char *value;
     int *free_value;
{
    char *ret, *t;
    const char *s;

    for (s = value; *s != '\0'; s++)
    {
	char c;

	c = *s;
	if (c == '\t'
	    || c == '\n'
	    || c == '\\'
	    || c == ' '
	    || c == '$')
	{
	    break;
	}
    }

    if (*s == '\0')
    {
	*free_value = 0;
	return (char *) value;
    }

    ret = xmalloc (strlen (value) * 4 + 1);
    *free_value = 1;

    for (s = value, t = ret; *s != '\0'; s++, t++)
    {
	switch (*s)
	{
	default:
	    *t = *s;
	    break;
	case '\t':
	    *t++ = '\\';
	    *t = 't';
	    break;
	case '\n':
	    *t++ = '\\';
	    *t = 'n';
	    break;
	case '\\':
	    *t++ = '\\';
	    *t = '\\';
	    break;
	case ' ':
	    *t++ = '\\';
	    *t++ = '0';
	    *t++ = '4';
	    *t = '0';
	    break;
	case '$':
	    *t++ = '\\';
	    *t++ = '0';
	    *t++ = '4';
	    *t = '4';
	    break;
	}
    }

    *t = '\0';

    return ret;
}

/* Expand RCS keywords in the memory buffer BUF of length LEN.  This
   applies to file RCS and version VERS.  If NAME is not NULL, and is
   not a numeric revision, then it is the symbolic tag used for the
   checkout.  EXPAND indicates how to expand the keywords.  This
   function sets *RETBUF and *RETLEN to the new buffer and length.
   This function may modify the buffer BUF.  If BUF != *RETBUF, then
   RETBUF is a newly allocated buffer.  */

static void
expand_keywords (rcs, ver, name, log, loglen, expand, buf, len, retbuf, retlen)
     RCSNode *rcs;
     RCSVers *ver;
     const char *name;
     const char *log;
     size_t loglen;
     enum kflag expand;
     char *buf;
     size_t len;
     char **retbuf;
     size_t *retlen;
{
    struct expand_buffer
    {
	struct expand_buffer *next;
	char *data;
	size_t len;
	int free_data;
    } *ebufs = NULL;
    struct expand_buffer *ebuf_last = NULL;
    size_t ebuf_len = 0;
    char *locker;
    char *srch, *srch_next;
    size_t srch_len;

    if (expand == KFLAG_O || expand == KFLAG_B)
    {
	*retbuf = buf;
	*retlen = len;
	return;
    }

    /* If we are using -kkvl, dig out the locker information if any.  */
    locker = NULL;
    if (expand == KFLAG_KVL)
    {
	Node *lock;
	lock = findnode (RCS_getlocks(rcs), ver->version);
	if (lock != NULL)
	    locker = xstrdup (lock->data);
    }

    /* RCS keywords look like $STRING$ or $STRING: VALUE$.  */
    srch = buf;
    srch_len = len;
    while ((srch_next = memchr (srch, '$', srch_len)) != NULL)
    {
	char *s, *send;
	size_t slen;
	const struct rcs_keyword *keyword;
	enum keyword kw;
	char *value;
	int free_value;
	char *sub;
	size_t sublen;

	srch_len -= (srch_next + 1) - srch;
	srch = srch_next + 1;

	/* Look for the first non alphabetic character after the '$'.  */
	send = srch + srch_len;
	for (s = srch; s < send; s++)
	    if (! isalpha (*s))
		break;

	/* If the first non alphabetic character is not '$' or ':',
           then this is not an RCS keyword.  */
	if (s == send || (*s != '$' && *s != ':'))
	    continue;

	/* See if this is one of the keywords.  */
	slen = s - srch;
	for (keyword = keywords; keyword->string != NULL; keyword++)
	{
	    if (keyword->len == slen
		&& strncmp (keyword->string, srch, slen) == 0)
	    {
		break;
	    }
	}
	if (keyword->string == NULL)
	    continue;

	kw = (enum keyword) (keyword - keywords);

	/* If the keyword ends with a ':', then the old value consists
           of the characters up to the next '$'.  If there is no '$'
           before the end of the line, though, then this wasn't an RCS
           keyword after all.  */
	if (*s == ':')
	{
	    for (; s < send; s++)
		if (*s == '$' || *s == '\n')
		    break;
	    if (s == send || *s != '$')
		continue;
	}

	/* At this point we must replace the string from SRCH to S
           with the expansion of the keyword KW.  */

	/* Get the value to use.  */
	free_value = 0;
	if (expand == KFLAG_K)
	    value = NULL;
	else
	{
	    switch (kw)
	    {
	    default:
		abort ();

	    case KEYWORD_AUTHOR:
		value = ver->author;
		break;

	    case KEYWORD_DATE:
		value = printable_date (ver->date);
		free_value = 1;
		break;

	    case KEYWORD_HEADER:
	    case KEYWORD_ID:
		{
		    char *path;
		    int free_path;
		    char *date;

		    if (kw == KEYWORD_HEADER)
			path = rcs->path;
		    else
			path = last_component (rcs->path);
		    path = escape_keyword_value (path, &free_path);
		    date = printable_date (ver->date);
		    value = xmalloc (strlen (path)
				     + strlen (ver->version)
				     + strlen (date)
				     + strlen (ver->author)
				     + strlen (ver->state)
				     + (locker == NULL ? 0 : strlen (locker))
				     + 20);

		    sprintf (value, "%s %s %s %s %s%s%s",
			     path, ver->version, date, ver->author,
			     ver->state,
			     locker != NULL ? " " : "",
			     locker != NULL ? locker : "");
		    if (free_path)
			free (path);
		    free (date);
		    free_value = 1;
		}
		break;

	    case KEYWORD_LOCKER:
		value = locker;
		break;

	    case KEYWORD_LOG:
	    case KEYWORD_RCSFILE:
		value = escape_keyword_value (last_component (rcs->path),
					      &free_value);
		break;

	    case KEYWORD_NAME:
		if (name != NULL && ! isdigit (*name))
		    value = (char *) name;
		else
		    value = NULL;
		break;

	    case KEYWORD_REVISION:
		value = ver->version;
		break;

	    case KEYWORD_SOURCE:
		value = escape_keyword_value (rcs->path, &free_value);
		break;

	    case KEYWORD_STATE:
		value = ver->state;
		break;
	    }
	}

	sub = xmalloc (keyword->len
		       + (value == NULL ? 0 : strlen (value))
		       + 10);
	if (expand == KFLAG_V)
	{
	    /* Decrement SRCH and increment S to remove the $
               characters.  */
	    --srch;
	    ++srch_len;
	    ++s;
	    sublen = 0;
	}
	else
	{
	    strcpy (sub, keyword->string);
	    sublen = strlen (keyword->string);
	    if (expand != KFLAG_K)
	    {
		sub[sublen] = ':';
		sub[sublen + 1] = ' ';
		sublen += 2;
	    }
	}
	if (value != NULL)
	{
	    strcpy (sub + sublen, value);
	    sublen += strlen (value);
	}
	if (expand != KFLAG_V && expand != KFLAG_K)
	{
	    sub[sublen] = ' ';
	    ++sublen;
	    sub[sublen] = '\0';
	}

	if (free_value)
	    free (value);

	/* The Log keyword requires special handling.  This behaviour
           is taken from RCS 5.7.  The special log message is what RCS
           uses for ci -k.  */
	if (kw == KEYWORD_LOG
	    && (sizeof "checked in with -k by " <= loglen
		|| strncmp (log, "checked in with -k by ",
			    sizeof "checked in with -k by " - 1) != 0))
	{
	    char *start;
	    char *leader;
	    size_t leader_len, leader_sp_len;
	    const char *logend;
	    const char *snl;
	    int cnl;
	    char *date;
	    const char *sl;

	    /* We are going to insert the trailing $ ourselves, before
               the log message, so we must remove it from S, if we
               haven't done so already.  */
	    if (expand != KFLAG_V)
		++s;

	    /* Find the start of the line.  */
	    start = srch;
	    while (start > buf && start[-1] != '\n')
		--start;

	    /* Copy the start of the line to use as a comment leader.  */
	    leader_len = srch - start;
	    if (expand != KFLAG_V)
		--leader_len;
	    leader = xmalloc (leader_len);
	    memcpy (leader, start, leader_len);
	    leader_sp_len = leader_len;
	    while (leader_sp_len > 0 && leader[leader_sp_len - 1] == ' ')
		--leader_sp_len;

	    /* RCS does some checking for an old style of Log here,
	       but we don't bother.  RCS issues a warning if it
	       changes anything.  */

	    /* Count the number of newlines in the log message so that
	       we know how many copies of the leader we will need.  */
	    cnl = 0;
	    logend = log + loglen;
	    for (snl = log; snl < logend; snl++)
		if (*snl == '\n')
		    ++cnl;

	    date = printable_date (ver->date);
	    sub = xrealloc (sub,
			    (sublen
			     + sizeof "Revision"
			     + strlen (ver->version)
			     + strlen (date)
			     + strlen (ver->author)
			     + loglen
			     + (cnl + 2) * leader_len
			     + 20));
	    if (expand != KFLAG_V)
	    {
		sub[sublen] = '$';
		++sublen;
	    }
	    sub[sublen] = '\n';
	    ++sublen;
	    memcpy (sub + sublen, leader, leader_len);
	    sublen += leader_len;
	    sprintf (sub + sublen, "Revision %s  %s  %s\n",
		     ver->version, date, ver->author);
	    sublen += strlen (sub + sublen);
	    free (date);

	    sl = log;
	    while (sl < logend)
	    {
		if (*sl == '\n')
		{
		    memcpy (sub + sublen, leader, leader_sp_len);
		    sublen += leader_sp_len;
		    sub[sublen] = '\n';
		    ++sublen;
		    ++sl;
		}
		else
		{
		    const char *slnl;

		    memcpy (sub + sublen, leader, leader_len);
		    sublen += leader_len;
		    for (slnl = sl; slnl < logend && *slnl != '\n'; ++slnl)
			;
		    if (slnl < logend)
			++slnl;
		    memcpy (sub + sublen, sl, slnl - sl);
		    sublen += slnl - sl;
		    sl = slnl;
		}
	    }

	    memcpy (sub + sublen, leader, leader_sp_len);
	    sublen += leader_sp_len;

	    free (leader);
	}

	/* Now SUB contains a string which is to replace the string
	   from SRCH to S.  SUBLEN is the length of SUB.  */

	if (srch + sublen == s)
	{
	    memcpy (srch, sub, sublen);
	    free (sub);
	}
	else
	{
	    struct expand_buffer *ebuf;

	    /* We need to change the size of the buffer.  We build a
               list of expand_buffer structures.  Each expand_buffer
               structure represents a portion of the final output.  We
               concatenate them back into a single buffer when we are
               done.  This minimizes the number of potentially large
               buffer copies we must do.  */

	    if (ebufs == NULL)
	    {
		ebufs = (struct expand_buffer *) xmalloc (sizeof *ebuf);
		ebufs->next = NULL;
		ebufs->data = buf;
		ebufs->free_data = 0;
		ebuf_len = srch - buf;
		ebufs->len = ebuf_len;
		ebuf_last = ebufs;
	    }
	    else
	    {
		assert (srch >= ebuf_last->data);
		assert (srch <= ebuf_last->data + ebuf_last->len);
		ebuf_len -= ebuf_last->len - (srch - ebuf_last->data);
		ebuf_last->len = srch - ebuf_last->data;
	    }

	    ebuf = (struct expand_buffer *) xmalloc (sizeof *ebuf);
	    ebuf->data = sub;
	    ebuf->len = sublen;
	    ebuf->free_data = 1;
	    ebuf->next = NULL;
	    ebuf_last->next = ebuf;
	    ebuf_last = ebuf;
	    ebuf_len += sublen;

	    ebuf = (struct expand_buffer *) xmalloc (sizeof *ebuf);
	    ebuf->data = s;
	    ebuf->len = srch_len - (s - srch);
	    ebuf->free_data = 0;
	    ebuf->next = NULL;
	    ebuf_last->next = ebuf;
	    ebuf_last = ebuf;
	    ebuf_len += srch_len - (s - srch);
	}

	srch_len -= (s - srch);
	srch = s;
    }

    if (locker != NULL)
	free (locker);

    if (ebufs == NULL)
    {
	*retbuf = buf;
	*retlen = len;
    }
    else
    {
	char *ret;

	ret = xmalloc (ebuf_len);
	*retbuf = ret;
	*retlen = ebuf_len;
	while (ebufs != NULL)
	{
	    struct expand_buffer *next;

	    memcpy (ret, ebufs->data, ebufs->len);
	    ret += ebufs->len;
	    if (ebufs->free_data)
		free (ebufs->data);
	    next = ebufs->next;
	    free (ebufs);
	    ebufs = next;
	}
    }
}

/* Check out a revision from an RCS file.

   If PFN is not NULL, then ignore WORKFILE and SOUT.  Call PFN zero
   or more times with the contents of the file.  CALLERDAT is passed,
   uninterpreted, to PFN.  (The current code will always call PFN
   exactly once for a non empty file; however, the current code
   assumes that it can hold the entire file contents in memory, which
   is not a good assumption, and might change in the future).

   Otherwise, if WORKFILE is not NULL, check out the revision to
   WORKFILE.  However, if WORKFILE is not NULL, and noexec is set,
   then don't do anything.

   Otherwise, if WORKFILE is NULL, check out the revision to SOUT.  If
   SOUT is RUN_TTY, then write the contents of the revision to
   standard output.  When using SOUT, the output is generally a
   temporary file; don't bother to get the file modes correct.

   REV is the numeric revision to check out.  It may be NULL, which
   means to check out the head of the default branch.

   If NAMETAG is not NULL, and is not a numeric revision, then it is
   the tag that should be used when expanding the RCS Name keyword.

   OPTIONS is a string such as "-kb" or "-kv" for keyword expansion
   options.  It may be NULL to use the default expansion mode of the
   file, typically "-kkv".  */

int
RCS_checkout (rcs, workfile, rev, nametag, options, sout, pfn, callerdat)
     RCSNode *rcs;
     char *workfile;
     char *rev;
     char *nametag;
     char *options;
     char *sout;
     RCSCHECKOUTPROC pfn;
     void *callerdat;
{
    int free_rev = 0;
    enum kflag expand;
    FILE *fp;
    struct stat sb;
    char *key;
    char *value;
    size_t len;
    int free_value = 0;
    char *log = NULL;
    size_t loglen;
    FILE *ofp;

    if (trace)
    {
	(void) fprintf (stderr, "%s-> checkout (%s, %s, %s, %s)\n",
#ifdef SERVER_SUPPORT
			server_active ? "S" : " ",
#else
			"",
#endif
			rcs->path,
			rev != NULL ? rev : "",
			options != NULL ? options : "",
			(pfn != NULL ? "(function)"
			 : (workfile != NULL
			    ? workfile
			    : (sout != RUN_TTY ? sout : "(stdout)"))));
    }

    assert (rev == NULL || isdigit (*rev));

    if (noexec && workfile != NULL)
	return 0;

    assert (sout == RUN_TTY || workfile == NULL);
    assert (pfn == NULL || (sout == RUN_TTY && workfile == NULL));

    /* Some callers, such as Checkin or remove_file, will pass us a
       branch.  */
    if (rev != NULL && (numdots (rev) & 1) == 0)
    {
	rev = RCS_getbranch (rcs, rev, 1);
	if (rev == NULL)
	    error (1, 0, "internal error: bad branch tag in checkout");
	free_rev = 1;
    }

    if (rev == NULL || strcmp (rev, rcs->head) == 0)
    {
	int gothead;

	/* We want the head revision.  Try to read it directly.  */

	if (rcs->flags & PARTIAL)
	    RCS_reparsercsfile (rcs, &fp);
	else
	{
	    fp = CVS_FOPEN (rcs->path, FOPEN_BINARY_READ);
	    if (fp == NULL)
		error (1, 0, "unable to reopen `%s'", rcs->path);
	    if (fseek (fp, rcs->delta_pos, SEEK_SET) != 0)
		error (1, 0, "cannot fseek RCS file");
	}

	gothead = 0;
	getrcsrev (fp, &key);
	while (getrcskey (fp, &key, &value, &len) >= 0)
	{
	    if (strcmp (key, "log") == 0)
	    {
		log = xmalloc (len);
		memcpy (log, value, len);
		loglen = len;
	    }
	    if (strcmp (key, "text") == 0)
	    {
		gothead = 1;
		break;
	    }
	}

	if (! gothead)
	{
	    error (0, 0, "internal error: cannot find head text");
	    if (free_rev)
		free (rev);
	    return 1;
	}

	if (fstat (fileno (fp), &sb) < 0)
	    error (1, errno, "cannot fstat %s", rcs->path);

	if (fclose (fp) < 0)
	    error (0, errno, "cannot close %s", rcs->path);
    }
    else
    {
	/* It isn't the head revision of the trunk.  We'll need to
	   walk through the deltas.  */

	fp = NULL;
	if (rcs->flags & PARTIAL)
	    RCS_reparsercsfile (rcs, &fp);

	if (fp == NULL)
	{
	    /* If RCS_deltas didn't close the file, we could use fstat
	       here too.  Probably should change it thusly....  */
	    if (stat (rcs->path, &sb) < 0)
		error (1, errno, "cannot stat %s", rcs->path);
	}
	else
	{
	    if (fstat (fileno (fp), &sb) < 0)
		error (1, errno, "cannot fstat %s", rcs->path);
	}

	RCS_deltas (rcs, fp, rev, RCS_FETCH, &value, &len, &log, &loglen);
	free_value = 1;
    }

    /* If OPTIONS is NULL or the empty string, then the old code would
       invoke the RCS co program with no -k option, which means that
       co would use the string we have stored in rcs->expand.  */
    if ((options == NULL || options[0] == '\0') && rcs->expand == NULL)
	expand = KFLAG_KV;
    else
    {
	const char *ouroptions;
	const char * const *cpp;

	if (options != NULL && options[0] != '\0')
	{
	    assert (options[0] == '-' && options[1] == 'k');
	    ouroptions = options + 2;
	}
	else
	    ouroptions = rcs->expand;

	for (cpp = kflags; *cpp != NULL; cpp++)
	    if (strcmp (*cpp, ouroptions) == 0)
		break;

	if (*cpp != NULL)
	    expand = (enum kflag) (cpp - kflags);
	else
	{
	    error (0, 0,
		   "internal error: unsupported substitution string -k%s",
		   ouroptions);
	    expand = KFLAG_KV;
	}
    }

    if (expand != KFLAG_O && expand != KFLAG_B)
    {
	Node *p;
	char *newvalue;

	p = findnode (rcs->versions, rev == NULL ? rcs->head : rev);
	if (p == NULL)
	    error (1, 0, "internal error: no revision information for %s",
		   rev == NULL ? rcs->head : rev);

	expand_keywords (rcs, (RCSVers *) p->data, nametag, log, loglen,
			 expand, value, len, &newvalue, &len);

	if (newvalue != value)
	{
	    if (free_value)
		free (value);
	    value = newvalue;
	    free_value = 1;
	}
    }

    if (log != NULL)
    {
	free (log);
	log = NULL;
    }

    if (pfn != NULL)
    {
	/* The PFN interface is very simple to implement right now, as
           we always have the entire file in memory.  */
	if (len != 0)
	    pfn (callerdat, value, len);
    }
    else
    {
	if (workfile == NULL)
	{
	    if (sout == RUN_TTY)
		ofp = stdout;
	    else
	    {
		ofp = CVS_FOPEN (sout, expand == KFLAG_B ? "wb" : "w");
		if (ofp == NULL)
		    error (1, errno, "cannot open %s", sout);
	    }
	}
	else
	{
	    ofp = CVS_FOPEN (workfile, expand == KFLAG_B ? "wb" : "w");
	    if (ofp == NULL)
		error (1, errno, "cannot open %s", workfile);
	}

	if (workfile == NULL && sout == RUN_TTY)
	{
	    if (expand == KFLAG_B)
		cvs_output_binary (value, len);
	    else
	    {
		/* cvs_output requires the caller to check for zero
		   length.  */
		if (len > 0)
		    cvs_output (value, len);
	    }
	}
	else
	{
	    /* NT 4.0 is said to have trouble writing 2099999 bytes
	       (for example) in a single fwrite.  So break it down
	       (there is no need to be writing that much at once
	       anyway; it is possible that LARGEST_FWRITE should be
	       somewhat larger for good performance, but for testing I
	       want to start with a small value until/unless a bigger
	       one proves useful).  */
#define LARGEST_FWRITE 8192
	    size_t nleft = len;
	    size_t nstep = (len < LARGEST_FWRITE ? len : LARGEST_FWRITE);
	    char *p = value;

	    while (nleft > 0)
	    {
		if (fwrite (p, 1, nstep, ofp) != nstep)
		    error (1, errno, "cannot write %s",
			   (workfile != NULL
			    ? workfile
			    : (sout != RUN_TTY ? sout : "stdout")));
		p += nstep;
		nleft -= nstep;
		if (nleft < nstep)
		    nstep = nleft;
	    }
	}

	if (workfile != NULL)
	{
	    if (fclose (ofp) < 0)
		error (1, errno, "cannot close %s", workfile);
	    if (chmod (workfile,
		       sb.st_mode & ~(S_IWRITE | S_IWGRP | S_IWOTH)) < 0)
		error (0, errno, "cannot change mode of file %s",
		       workfile);
	}
	else if (sout != RUN_TTY)
	{
	    if (fclose (ofp) < 0)
		error (1, errno, "cannot close %s", sout);
	}
    }

    if (free_value)
	free (value);
    if (free_rev)
	free (rev);

    return 0;
}

static RCSVers *RCS_findlock_or_tip PROTO ((RCSNode *rcs));

/* Find the delta currently locked by the user.  From the `ci' man page:

	"If rev is omitted, ci tries to  derive  the  new  revision
	 number  from  the  caller's  last lock.  If the caller has
	 locked the tip revision of a branch, the new  revision  is
	 appended  to  that  branch.   The  new  revision number is
	 obtained by incrementing the tip revision number.  If  the
	 caller  locked a non-tip revision, a new branch is started
	 at that revision by incrementing the highest branch number
	 at  that  revision.   The default initial branch and level
	 numbers are 1.

	 If rev is omitted and the caller has no lock, but owns the
	 file  and  locking is not set to strict, then the revision
	 is appended to the default branch (normally the trunk; see
	 the -b option of rcs(1))."

   RCS_findlock_or_tip finds the unique revision locked by the caller
   and returns its delta node.  If the caller has not locked any
   revisions (and is permitted to commit to an unlocked delta, as
   described above), return the tip of the default branch. */

static RCSVers *
RCS_findlock_or_tip (rcs)
    RCSNode *rcs;
{
    char *user = getcaller();
    Node *lock, *p;
    List *locklist;

    /* Find unique delta locked by caller. This code is very similar
       to the code in RCS_unlock -- perhaps it could be abstracted
       into a RCS_findlock function. */
    locklist = RCS_getlocks (rcs);
    lock = NULL;
    for (p = locklist->list->next; p != locklist->list; p = p->next)
    {
	if (strcmp (p->data, user) == 0)
	{
	    if (lock != NULL)
	    {
		error (0, 0, "\
%s: multiple revisions locked by %s; please specify one", rcs->path, user);
		return NULL;
	    }
	    lock = p;
	}
    }

    if (lock != NULL)
    {
	/* Found an old lock, but check that the revision still exists. */
	p = findnode (rcs->versions, lock->key);
	if (p == NULL)
	{
	    error (0, 0, "%s: can't unlock nonexistent revision %s",
		   rcs->path,
		   lock->key);
	    return NULL;
	}
	return (RCSVers *) p->data;
    }

    /* No existing lock.  The RCS rule is that this is an error unless
       locking is nonstrict AND the file is owned by the current
       user.  Trying to determine the latter is a portability nightmare
       in the face of NT, VMS, AFS, and other systems with non-unix-like
       ideas of users and owners.  In the case of CVS, we should never get
       here (as long as the traditional behavior of making sure to call
       RCS_lock persists).  Anyway, we skip the RCS error checks
       and just return the default branch or head.  The reasoning is that
       those error checks are to make users lock before a checkin, and we do
       that in other ways if at all anyway (e.g. rcslock.pl).  */

    p = findnode (rcs->versions, RCS_getbranch (rcs, rcs->branch, 0));
    return (RCSVers *) p->data;
}

/* Revision number string, R, must contain a `.'.
   Return a newly-malloc'd copy of the prefix of R up
   to but not including the final `.'.  */

static char *
truncate_revnum (r)
    const char *r;
{
    size_t len;
    char *new_r;
    char *dot = strrchr (r, '.');

    assert (dot);
    len = dot - r;
    new_r = xmalloc (len + 1);
    memcpy (new_r, r, len);
    *(new_r + len) = '\0';
    return new_r;
}

/* Revision number string, R, must contain a `.'.
   R must be writable.  Replace the rightmost `.' in R with
   the NUL byte and return a pointer to that NUL byte.  */

static char *
truncate_revnum_in_place (r)
    char *r;
{
    char *dot = strrchr (r, '.');
    assert (dot);
    *dot = '\0';
    return dot;
}

/* Revision number strings, R and S, must each contain a `.'.
   R and S must be writable and must have the same number of dots.
   Truncate R and S for the comparison, then restored them to their
   original state.
   Return the result (see compare_revnums) of comparing R and S
   ignoring differences in any component after the rightmost `.'.  */

static int
compare_truncated_revnums (r, s)
    char *r;
    char *s;
{
    char *r_dot = truncate_revnum_in_place (r);
    char *s_dot = truncate_revnum_in_place (s);
    int cmp;

    assert (numdots (r) == numdots (s));

    cmp = compare_revnums (r, s);

    *r_dot = '.';
    *s_dot = '.';

    return cmp;
}

/* Return a malloc'd copy of the string representing the highest branch
   number on BRANCHNODE.  If there are no branches on BRANCHNODE, return NULL.
   FIXME: isn't the max rev always the last one?
   If so, we don't even need a loop.  */

static char *
max_rev (const RCSVers *branchnode)
{
    Node *head;
    Node *bp;
    char *max;

    if (branchnode->branches == NULL)
    {
        return NULL;
    }

    max = NULL;
    head = branchnode->branches->list;
    for (bp = head->next; bp != head; bp = bp->next)
    {
	if (max == NULL || compare_truncated_revnums (max, bp->key) < 0)
	{
	    max = bp->key;
	}
    }
    assert (max);

    return truncate_revnum (max);
}

/* Create BRANCH in RCS's delta tree.  BRANCH may be either a branch
   number or a revision number.  In the former case, create the branch
   with the specified number; in the latter case, create a new branch
   rooted at node BRANCH with a higher branch number than any others.
   Return the number of the tip node on the new branch. */

static char *
RCS_addbranch (rcs, branch)
    RCSNode *rcs;
    const char *branch;
{
    char *branchpoint, *newrevnum;
    Node *nodep, *bp;
    Node *marker;
    RCSVers *branchnode;

    /* Append to end by default.  */
    marker = NULL;

    branchpoint = xstrdup (branch);
    if ((numdots (branchpoint) & 1) == 0)
    {
	truncate_revnum_in_place (branchpoint);
    }

    /* Find the branch rooted at BRANCHPOINT. */
    nodep = findnode (rcs->versions, branchpoint);
    if (nodep == NULL)
    {
	error (0, 0, "%s: can't find branch point %s", rcs->path, branchpoint);
	return NULL;
    }
    branchnode = (RCSVers *) nodep->data;

    /* If BRANCH was a full branch number, make sure it is higher than MAX. */
    if ((numdots (branch) & 1) == 1)
    {
	if (branchnode->branches == NULL)
	{
	    /* We have to create the first branch on this node, which means
	       appending ".2" to the revision number. */
	    newrevnum = (char *) xmalloc (strlen (branch) + 3);
	    strcpy (newrevnum, branch);
	    strcat (newrevnum, ".2");
	}
	else
	{
	    char *max = max_rev (branchnode);
	    assert (max);
	    newrevnum = increment_revnum (max);
	    free (max);
	}
    }
    else
    {
	newrevnum = xstrdup (branch);

	if (branchnode->branches != NULL)
	{
	    Node *head;
	    Node *bp;

	    /* Find the position of this new branch in the sorted list
	       of branches.  */
	    head = branchnode->branches->list;
	    for (bp = head->next; bp != head; bp = bp->next)
	    {
		char *dot;
		int found_pos;

		/* The existing list must be sorted on increasing revnum.  */
		assert (bp->next == head
			|| compare_truncated_revnums (bp->key,
						      bp->next->key) < 0);
		dot = truncate_revnum_in_place (bp->key);
		found_pos = (compare_revnums (branch, bp->key) < 0);
		*dot = '.';

		if (found_pos)
		{
		    break;
		}
	    }
	    marker = bp;
	}
    }

    newrevnum = (char *) xrealloc (newrevnum, strlen (newrevnum) + 3);
    strcat (newrevnum, ".1");

    /* Add this new revision number to BRANCHPOINT's branches list. */
    if (branchnode->branches == NULL)
	branchnode->branches = getlist();
    bp = getnode();
    bp->key = xstrdup (newrevnum);

    /* Append to the end of the list by default, that is, just before
       the header node, `list'.  */
    if (marker == NULL)
	marker = branchnode->branches->list;

    {
	int fail;
	fail = insert_before (branchnode->branches, marker, bp);
	assert (!fail);
    }

    return newrevnum;
}

/* Check in to RCSFILE with revision REV (which must be greater than the
   largest revision) and message MESSAGE (which is checked for legality).
   If FLAGS & RCS_FLAGS_DEAD, check in a dead revision.  If FLAGS &
   RCS_FLAGS_QUIET, tell ci to be quiet.  If FLAGS & RCS_FLAGS_MODTIME,
   use the working file's modification time for the checkin time.
   WORKFILE is the working file to check in from, or NULL to use the usual
   RCS rules for deriving it from the RCSFILE.
   
   Return value is -1 for error (and errno is set to indicate the
   error), positive for error (and an error message has been printed),
   or zero for success.  */

/* TODO: RCS_checkin always unlinks the working file after checkin --
   then RCS_checkout checks it out again.  The logic should probably
   be reversed here. */

int
RCS_checkin (rcs, workfile, message, rev, flags)
    RCSNode *rcs;
    char *workfile;
    char *message;
    char *rev;
    int flags;
{
    RCSVers *delta, *commitpt;
    Deltatext *dtext;
    Node *nodep;
    char *tmpfile, *changefile, *chtext;
    char *diffopts;
    size_t bufsize;
    int buflen, chtextlen;
    int status, checkin_quiet, allocated_workfile;
    struct tm *ftm;
    time_t modtime;

    commitpt = NULL;

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    /* Get basename of working file.  Is there a library function to
       do this?  I couldn't find one. -twp */
    allocated_workfile = 0;
    if (workfile == NULL)
    {
	char *p;
	int extlen = strlen (RCSEXT);
	workfile = xstrdup (last_component (rcs->path));
	p = workfile + (strlen (workfile) - extlen);
	assert (strncmp (p, RCSEXT, extlen) == 0);
	*p = '\0';
	allocated_workfile = 1;
    }

    checkin_quiet = flags & RCS_FLAGS_QUIET;
    if (!checkin_quiet)
    {
	cvs_output (rcs->path, 0);
	cvs_output ("  <--  ", 7);
	cvs_output (workfile, 0);
	cvs_output ("\n", 1);
    }

    /* Create new delta node. */
    delta = (RCSVers *) xmalloc (sizeof (RCSVers));
    memset (delta, 0, sizeof (RCSVers));
    delta->author = xstrdup (getcaller ());
    if (flags & RCS_FLAGS_MODTIME)
    {
	struct stat ws;
	if (stat (workfile, &ws) < 0)
	{
	    error (1, errno, "cannot stat %s", workfile);
	}
	modtime = ws.st_mtime;
    }
    else
	(void) time (&modtime);
    ftm = gmtime (&modtime);
    delta->date = (char *) xmalloc (MAXDATELEN);
    (void) sprintf (delta->date, DATEFORM,
		    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
		    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
    if (flags & RCS_FLAGS_DEAD)
    {
	delta->state = xstrdup (RCSDEAD);
	delta->dead = 1;
    }
    else
	delta->state = xstrdup ("Exp");

    /* Create a new deltatext node. */
    dtext = (Deltatext *) xmalloc (sizeof (Deltatext));
    memset (dtext, 0, sizeof (Deltatext));

    dtext->log = make_message_rcslegal (message);

    /* If the delta tree is empty, then there's nothing to link the
       new delta into.  So make a new delta tree, snarf the working
       file contents, and just write the new RCS file. */
    if (rcs->head == NULL)
    {
	char *newrev;
	FILE *fout;

	/* Figure out what the first revision number should be. */
	if (rev == NULL || *rev == '\0')
	    newrev = xstrdup ("1.1");
	else if (numdots (rev) == 0)
	{
	    newrev = (char *) xmalloc (strlen (rev) + 3);
	    strcpy (newrev, rev);
	    strcat (newrev, ".1");
	}
	else
	    newrev = xstrdup (rev);

	/* Don't need to xstrdup NEWREV because it's already dynamic, and
	   not used for anything else.  (Don't need to free it, either.) */
	rcs->head = newrev;
	delta->version = xstrdup (newrev);
	nodep = getnode();
	nodep->type = RCSVERS;
	nodep->key = xstrdup (newrev);
	nodep->data = (char *) delta;
	(void) addnode (rcs->versions, nodep);

	dtext->version = xstrdup (newrev);
	bufsize = 0;
	get_file(workfile, workfile, "r", &dtext->text, &bufsize, &dtext->len);

	if (!checkin_quiet)
	{
	    cvs_output ("initial revision: ", 0);
	    cvs_output (rcs->head, 0);
	    cvs_output ("\n", 1);
	}

	fout = rcs_internal_lockfile (rcs->path);
	RCS_putadmin (rcs, fout);
	RCS_putdtree (rcs, rcs->head, fout);
	RCS_putdesc (rcs, fout);
	rcs->delta_pos = ftell (fout);
	if (rcs->delta_pos == -1)
	    error (1, errno, "cannot ftell for %s", rcs->path);
	putdeltatext (fout, dtext);
	rcs_internal_unlockfile (fout, rcs->path);
	freedeltatext (dtext);

	/* Removing the file here is an RCS user-visible behavior which
	   we almost surely do not need in the CVS case.  In fact, getting
	   rid of it should clean up link_file and friends in import.c.  */
	if (unlink_file (workfile) < 0)
	    /* FIXME-update-dir: message does not include update_dir.  */
	    error (0, errno, "cannot remove %s", workfile);

	if (!checkin_quiet)
	    cvs_output ("done\n", 5);

	return 0;
    }

    /* Derive a new revision number.  From the `ci' man page:

	 "If rev  is  a revision number, it must be higher than the
	 latest one on the branch to which  rev  belongs,  or  must
	 start a new branch.

	 If  rev is a branch rather than a revision number, the new
	 revision is appended to that branch.  The level number  is
	 obtained  by  incrementing the tip revision number of that
	 branch.  If rev  indicates  a  non-existing  branch,  that
	 branch  is  created  with  the  initial  revision numbered
	 rev.1."

       RCS_findlock_or_tip handles the case where REV is omitted.
       RCS 5.7 also permits REV to be "$" or to begin with a dot, but
       we do not address those cases -- every routine that calls
       RCS_checkin passes it a numeric revision. */

    if (rev == NULL || *rev == '\0')
    {
	/* Figure out where the commit point is by looking for locks.
	   If the commit point is at the tip of a branch (or is the
	   head of the delta tree), then increment its revision number
	   to obtain the new revnum.  Otherwise, start a new
	   branch. */
	commitpt = RCS_findlock_or_tip (rcs);
	if (commitpt == NULL)
	{
	    status = 1;
	    goto checkin_done;
	}
	else if (commitpt->next == NULL
		 || strcmp (commitpt->version, rcs->head) == 0)
	    delta->version = increment_revnum (commitpt->version);
	else
	    delta->version = RCS_addbranch (rcs, commitpt->version);
    }
    else
    {
	/* REV is either a revision number or a branch number.  Find the
	   tip of the target branch. */
	char *branch, *tip, *newrev, *p;
	int dots, isrevnum;

	assert (isdigit(*rev));

	newrev = xstrdup (rev);
	dots = numdots (newrev);
	isrevnum = dots & 1;

	branch = xstrdup (rev);
	if (isrevnum)
	{
	    p = strrchr (branch, '.');
	    *p = '\0';
	}

	/* Find the tip of the target branch.  If we got a one- or two-digit
	   revision number, this will be the head of the tree.  Exception:
	   if rev is a single-field revision equal to the branch number of
	   the trunk (usually "1") then we want to treat it like an ordinary
	   branch revision. */
	if (dots == 0)
	{
	    tip = xstrdup (rcs->head);
	    if (atoi (tip) != atoi (branch))
	    {
		newrev = (char *) xrealloc (newrev, strlen (newrev) + 3);
		strcat (newrev, ".1");
		dots = isrevnum = 1;
	    }
	}
	else if (dots == 1)
	    tip = xstrdup (rcs->head);
	else
	    tip = RCS_getbranch (rcs, branch, 1);

	/* If the branch does not exist, and we were supplied an exact
	   revision number, signal an error.  Otherwise, if we were
	   given only a branch number, create it and set COMMITPT to
	   the branch point. */
	if (tip == NULL)
	{
	    if (isrevnum)
	    {
		error (0, 0, "%s: can't find branch point %s",
		       rcs->path, branch);
		free (branch);
		free (newrev);
		status = 1;
		goto checkin_done;
	    }
	    delta->version = RCS_addbranch (rcs, branch);
	    p = strrchr (branch, '.');
	    *p = '\0';
	    tip = xstrdup (branch);
	}
	else
	{
	    if (isrevnum)
	    {
		/* NEWREV must be higher than TIP. */
		if (compare_revnums (tip, newrev) >= 0)
		{
		    error (0, 0,
			   "%s: revision %s too low; must be higher than %s",
			   rcs->path,
			   newrev, tip);
		    free (branch);
		    free (newrev);
		    free (tip);
		    status = 1;
		    goto checkin_done;
		}
		delta->version = xstrdup (newrev);
	    }
	    else
		/* Just increment the tip number to get the new revision. */
		delta->version = increment_revnum (tip);
	}

	nodep = findnode (rcs->versions, tip);
	commitpt = (RCSVers *) nodep->data;

	free (branch);
	free (newrev);
	free (tip);
    }

    assert (delta->version != NULL);

    /* If COMMITPT is locked by us, break the lock.  If it's locked
       by someone else, signal an error. */
    nodep = findnode (RCS_getlocks (rcs), commitpt->version);
    if (nodep != NULL)
    {
	if (strcmp (nodep->data, delta->author) != 0)
	{
	    error (0, 0, "%s: revision %s locked by %s",
		   rcs->path,
		   nodep->key, nodep->data);
	    status = 1;
	    goto checkin_done;
	}
	delnode (nodep);
    }

    dtext->version = xstrdup (delta->version);

    /* Obtain the change text for the new delta.  If DELTA is to be the
       new head of the tree, then its change text should be the contents
       of the working file, and LEAFNODE's change text should be a diff.
       Else, DELTA's change text should be a diff between LEAFNODE and
       the working file. */

    tmpfile = cvs_temp_name();
    status = RCS_checkout (rcs, NULL, commitpt->version, NULL,
			   ((rcs->expand != NULL
			     && strcmp (rcs->expand, "b") == 0)
			    ? "-kb"
			    : "-ko"),
			   tmpfile,
			   (RCSCHECKOUTPROC)0, NULL);
    if (status != 0)
	error (1, status < 0 ? errno : 0,
	       "could not check out revision %s of `%s'",
	       commitpt->version, rcs->path);

    bufsize = buflen = 0;
    chtext = NULL;
    chtextlen = 0;
    changefile = cvs_temp_name();

    /* Diff options should include --binary if the RCS file has -kb set
       in its `expand' field. */
    diffopts = (rcs->expand != NULL && strcmp (rcs->expand, "b") == 0
		? "-a -n --binary"
		: "-a -n");

    if (strcmp (commitpt->version, rcs->head) == 0 &&
	numdots (delta->version) == 1)
    {
	/* If this revision is being inserted on the trunk, the change text
	   for the new delta should be the contents of the working file ... */
	bufsize = 0;
	get_file (workfile, workfile,
		  rcs->expand != NULL && strcmp (rcs->expand, "b") == 0 ? "rb" : "r",
		  &dtext->text, &bufsize, &dtext->len);

	/* ... and the change text for the old delta should be a diff. */
	commitpt->text = (Deltatext *) xmalloc (sizeof (Deltatext));
	memset (commitpt->text, 0, sizeof (Deltatext));

	bufsize = 0;
	switch (diff_exec (workfile, tmpfile, diffopts, changefile))
	{
	    case 0:
	    case 1:
		break;
	    case -1:
		/* FIXME-update-dir: message does not include update_dir.  */
		error (1, errno, "error diffing %s", workfile);
		break;
	    default:
		/* FIXME-update-dir: message does not include update_dir.  */
		error (1, 0, "error diffing %s", workfile);
		break;
	}

	/* OK, the text file case here is really dumb.  Logically
	   speaking we want diff to read the files in text mode,
	   convert them to the canonical form found in RCS files
	   (which, we hope at least, is independent of OS--always
	   bare linefeeds), and then work with change texts in that
	   format.  However, diff_exec both generates change
	   texts and produces output for user purposes (e.g. patch.c),
	   and there is no way to distinguish between the two cases.
	   So we actually implement the text file case by writing the
	   change text as a text file, then reading it as a text file.
	   This should cause no harm, but doesn't strike me as
	   immensely clean.  */
	get_file (changefile, changefile,
		  rcs->expand != NULL && strcmp (rcs->expand, "b") == 0 ? "rb" : "r",
		  &commitpt->text->text, &bufsize, &commitpt->text->len);

	/* If COMMITPT->TEXT->TEXT is NULL, it means that CHANGEFILE
	   was empty and that there are no differences between revisions.
	   In that event, we want to force RCS_rewrite to write an empty
	   string for COMMITPT's change text.  Leaving the change text
	   field set NULL won't work, since that means "preserve the original
	   change text for this delta." */
	if (commitpt->text->text == NULL)
	{
	    commitpt->text->text = xstrdup ("");
	    commitpt->text->len = 0;
	}
    }
    else
    {
	/* This file is not being inserted at the head, but on a side
	   branch somewhere.  Make a diff from the previous revision
	   to the working file. */
	switch (diff_exec (tmpfile, workfile, diffopts, changefile))
	{
	    case 0:
	    case 1:
		break;
	    case -1:
		/* FIXME-update-dir: message does not include update_dir.  */
		error (1, errno, "error diffing %s", workfile);
		break;
	    default:
		/* FIXME-update-dir: message does not include update_dir.  */
		error (1, 0, "error diffing %s", workfile);
		break;
	}
	/* See the comment above, at the other get_file invocation,
	   regarding binary vs. text.  */
	get_file (changefile, changefile, 
		  rcs->expand != NULL && strcmp (rcs->expand, "b") == 0 ? "rb" : "r",
		  &dtext->text, &bufsize,
		  &dtext->len);
	if (dtext->text == NULL)
	{
	    dtext->text = xstrdup ("");
	    dtext->len = 0;
	}
    }

    /* Update DELTA linkage.  It is important not to do this before
       the very end of RCS_checkin; if an error arises that forces
       us to abort checking in, we must not have malformed deltas
       partially linked into the tree.

       If DELTA and COMMITPT are on different branches, do nothing --
       DELTA is linked to the tree through COMMITPT->BRANCHES, and we
       don't want to change `next' pointers.

       Otherwise, if the nodes are both on the trunk, link DELTA to
       COMMITPT; otherwise, link COMMITPT to DELTA. */

    if (numdots (commitpt->version) == numdots (delta->version))
    {
	if (strcmp (commitpt->version, rcs->head) == 0)
	{
	    delta->next = rcs->head;
	    rcs->head = xstrdup (delta->version);
	}
	else
	    commitpt->next = xstrdup (delta->version);
    }

    /* Add DELTA to RCS->VERSIONS. */
    if (rcs->versions == NULL)
	rcs->versions = getlist();
    nodep = getnode();
    nodep->type = RCSVERS;
    nodep->key = xstrdup (delta->version);
    nodep->data = (char *) delta;
    (void) addnode (rcs->versions, nodep);
	
    /* Write the new RCS file, inserting the new delta at COMMITPT. */
    if (!checkin_quiet)
    {
	cvs_output ("new revision: ", 14);
	cvs_output (delta->version, 0);
	cvs_output ("; previous revision: ", 21);
	cvs_output (commitpt->version, 0);
	cvs_output ("\n", 1);
    }

    RCS_rewrite (rcs, dtext, commitpt->version);

    /* Removing the file here is an RCS user-visible behavior which
       we almost surely do not need in the CVS case.  In fact, getting
       rid of it should clean up link_file and friends in import.c.  */
    if (unlink_file (workfile) < 0)
	/* FIXME-update-dir: message does not include update_dir.  */
	error (1, errno, "cannot remove %s", workfile);
    if (unlink_file (tmpfile) < 0)
	error (0, errno, "cannot remove %s", tmpfile);
    if (unlink_file (changefile) < 0)
	error (0, errno, "cannot remove %s", changefile);

    if (!checkin_quiet)
	cvs_output ("done\n", 5);

 checkin_done:
    if (allocated_workfile)
	free (workfile);

    if (commitpt != NULL && commitpt->text != NULL)
    {
	freedeltatext (commitpt->text);
	commitpt->text = NULL;
    }

    freedeltatext (dtext);
    if (status != 0)
	free_rcsvers_contents (delta);

    return status;
}

/* This structure is passed between RCS_cmp_file and cmp_file_buffer.  */

struct cmp_file_data
{
    const char *filename;
    FILE *fp;
    int different;
};

/* Compare the contents of revision REV of RCS file RCS with the
   contents of the file FILENAME.  OPTIONS is a string for the keyword
   expansion options.  Return 0 if the contents of the revision are
   the same as the contents of the file, 1 if they are different.  */

int
RCS_cmp_file (rcs, rev, options, filename)
     RCSNode *rcs;
     char *rev;
     char *options;
     const char *filename;
{
    int binary;
    FILE *fp;
    struct cmp_file_data data;
    int retcode;

    if (options != NULL && options[0] != '\0')
	binary = (strcmp (options, "-kb") == 0);
    else
    {
	char *expand;

	expand = RCS_getexpand (rcs);
	if (expand != NULL && strcmp (expand, "b") == 0)
	    binary = 1;
	else
	    binary = 0;
    }

    fp = CVS_FOPEN (filename, binary ? FOPEN_BINARY_READ : "r");

    data.filename = filename;
    data.fp = fp;
    data.different = 0;

    retcode = RCS_checkout (rcs, (char *) NULL, rev, (char *) NULL,
			    options, RUN_TTY, cmp_file_buffer,
			    (void *) &data);

    /* If we have not yet found a difference, make sure that we are at
       the end of the file.  */
    if (! data.different)
    {
	if (getc (fp) != EOF)
	    data.different = 1;
    }

    fclose (fp);

    if (retcode != 0)
	return 1;

    return data.different;
}

/* This is a subroutine of RCS_cmp_file.  It is passed to
   RCS_checkout.  */

#define CMP_BUF_SIZE (8 * 1024)

static void
cmp_file_buffer (callerdat, buffer, len)
     void *callerdat;
     const char *buffer;
     size_t len;
{
    struct cmp_file_data *data = (struct cmp_file_data *) callerdat;
    char *filebuf;

    /* If we've already found a difference, we don't need to check
       further.  */
    if (data->different)
	return;

    filebuf = xmalloc (len > CMP_BUF_SIZE ? CMP_BUF_SIZE : len);

    while (len > 0)
    {
	size_t checklen;

	checklen = len > CMP_BUF_SIZE ? CMP_BUF_SIZE : len;
	if (fread (filebuf, 1, checklen, data->fp) != checklen)
	{
	    if (ferror (data->fp))
		error (1, errno, "cannot read file %s for comparing",
		       data->filename);
	    data->different = 1;
	    free (filebuf);
	    return;
	}

	if (memcmp (filebuf, buffer, checklen) != 0)
	{
	    data->different = 1;
	    free (filebuf);
	    return;
	}

	buffer += checklen;
	len -= checklen;
    }

    free (filebuf);
}

/* For RCS file RCS, make symbolic tag TAG point to revision REV.
   This validates that TAG is OK for a user to use.  Return value is
   -1 for error (and errno is set to indicate the error), positive for
   error (and an error message has been printed), or zero for success.  */

int
RCS_settag (rcs, tag, rev)
    RCSNode *rcs;
    const char *tag;
    const char *rev;
{
    List *symbols;
    Node *node;

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    /* FIXME: This check should be moved to RCS_check_tag.  There is no
       reason for it to be here.  */
    if (strcmp (tag, TAG_BASE) == 0
	|| strcmp (tag, TAG_HEAD) == 0)
    {
	/* Print the name of the tag might be considered redundant
	   with the caller, which also prints it.  Perhaps this helps
	   clarify why the tag name is considered reserved, I don't
	   know.  */
	error (0, 0, "Attempt to add reserved tag name %s", tag);
	return 1;
    }

    /* A revision number of NULL means use the head or default branch.
       If rev is not NULL, it may be a symbolic tag or branch number;
       expand it to the correct numeric revision or branch head. */
    if (rev == NULL)
	rev = rcs->branch ? rcs->branch : rcs->head;

    /* At this point rcs->symbol_data may not have been parsed.
       Calling RCS_symbols will force it to be parsed into a list
       which we can easily manipulate.  */
    symbols = RCS_symbols (rcs);
    if (symbols == NULL)
    {
	symbols = getlist ();
	rcs->symbols = symbols;
    }
    node = findnode (symbols, tag);
    if (node != NULL)
    {
	free (node->data);
	node->data = xstrdup (rev);
    }
    else
    {
	node = getnode ();
	node->key = xstrdup (tag);
	node->data = xstrdup (rev);
	(void) addnode_at_front (symbols, node);
    }

    return 0;
}

/* Delete the symbolic tag TAG from the RCS file RCS.  Return 0 if
   the tag was found (and removed), or 1 if it was not present.  (In
   either case, the tag will no longer be in RCS->SYMBOLS.) */

int
RCS_deltag (rcs, tag)
    RCSNode *rcs;
    const char *tag;
{
    List *symbols;
    Node *node;
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    symbols = RCS_symbols (rcs);
    if (symbols == NULL)
	return 1;

    node = findnode (symbols, tag);
    if (node == NULL)
	return 1;

    delnode (node);

    return 0;
}

/* Set the default branch of RCS to REV.  */

int
RCS_setbranch (rcs, rev)
     RCSNode *rcs;
     const char *rev;
{
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    if (rev == NULL && rcs->branch == NULL)
	return 0;
    if (rev != NULL && rcs->branch != NULL && strcmp (rev, rcs->branch) == 0)
	return 0;

    if (rcs->branch != NULL)
	free (rcs->branch);
    rcs->branch = xstrdup (rev);

    return 0;
}

/* Lock revision REV.  LOCK_QUIET is 1 to suppress output.  FIXME:
   This is only required because the RCS ci program requires a lock.
   If we eventually do the checkin ourselves, this can become a no-op.  */
/* FIXME-twp: if a lock owned by someone else is broken, should this
   send mail to the lock owner?  Prompt user?  It seems like such an
   obscure situation for CVS as almost not worth worrying much
   about. */

int
RCS_lock (rcs, rev, lock_quiet)
     RCSNode *rcs;
     const char *rev;
     int lock_quiet;
{
    List *locks;
    Node *p;
    char *user;
    char *xrev = NULL;

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    locks = RCS_getlocks (rcs);
    if (locks == NULL)
	locks = rcs->locks = getlist();
    user = getcaller();

    /* A revision number of NULL means lock the head or default branch. */
    if (rev == NULL)
	xrev = RCS_head (rcs);

    /* If rev is a branch number, lock the latest revision on that
       branch. I think that if the branch doesn't exist, it's
       okay to return 0 -- that just means that the branch is new,
       so we don't need to lock it anyway. -twp */
    else if (RCS_nodeisbranch (rcs, rev))
    {
	xrev = RCS_getbranch (rcs, (char *) rev, 1);
	if (xrev == NULL)
	{
	    if (!lock_quiet)
		error (0, 0, "%s: branch %s absent", rcs->path, rev);
	    return 1;
	}
    }

    if (xrev == NULL)
	xrev = xstrdup (rev);

    /* Make sure that the desired revision exists.  Technically,
       we can update the locks list without even checking this,
       but RCS 5.7 did this.  And it can't hurt. */
    if (findnode (rcs->versions, xrev) == NULL)
    {
	if (!lock_quiet)
	    error (0, 0, "%s: revision %s absent", rcs->path, xrev);
	free (xrev);
	return 1;
    }

    /* Is this rev already locked? */
    p = findnode (locks, xrev);
    if (p != NULL)
    {
	if (strcmp (p->data, user) == 0)
	{
	    /* We already own the lock on this revision, so do nothing. */
	    free (xrev);
	    return 0;
	}

	/* Break the lock. */	    
	if (!lock_quiet)
	{
	    cvs_output (rev, 0);
	    cvs_output (" unlocked\n", 0);
	}
	delnode (p);
    }

    /* Create a new lock. */
    p = getnode();
    p->key = xrev;	/* already xstrdupped */
    p->data = xstrdup (getcaller());
    (void) addnode_at_front (locks, p);

    if (!lock_quiet)
    {
	cvs_output (xrev, 0);
	cvs_output (" locked\n", 0);
    }

    return 0;
}

/* Unlock revision REV.  UNLOCK_QUIET is 1 to suppress output.  FIXME:
   Like RCS_lock, this can become a no-op if we do the checkin
   ourselves.

   If REV is not null and is locked by someone else, break their
   lock and notify them.  It is an open issue whether RCS_unlock
   queries the user about whether or not to break the lock. */

int
RCS_unlock (rcs, rev, unlock_quiet)
     RCSNode *rcs;
     const char *rev;
     int unlock_quiet;
{
    Node *lock;
    List *locks;
    char *user;
    char *xrev = NULL;

    user = getcaller();
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    /* If rev is NULL, unlock the latest revision (first in
       rcs->locks) held by the caller. */
    if (rev == NULL)
    {
	Node *p;

	/* No-ops: attempts to unlock an empty tree or an unlocked file. */
	if (rcs->head == NULL)
	{
	    if (!unlock_quiet)
		cvs_outerr ("can't unlock an empty tree\n", 0);
	    return 0;
	}

	locks = RCS_getlocks (rcs);
	if (locks == NULL)
	{
	    if (!unlock_quiet)
		cvs_outerr ("No locks are set.\n", 0);
	    return 0;
	}

	lock = NULL;
	for (p = locks->list->next; p != locks->list; p = p->next)
	{
	    if (strcmp (p->data, user) == 0)
	    {
		if (lock != NULL)
		{
		    if (!unlock_quiet)
			error (0, 0, "\
%s: multiple revisions locked by %s; please specify one", rcs->path, user);
		    return 1;
		}
		lock = p;
	    }
	}
	if (lock == NULL)
	    return 0;	/* no lock found, ergo nothing to do */
	xrev = xstrdup (lock->key);
    }
    else if (RCS_nodeisbranch (rcs, rev))
    {
	/* If rev is a branch number, unlock the latest revision on that
	   branch. */
	xrev = RCS_getbranch (rcs, (char *) rev, 1);
	if (xrev == NULL)
	{
	    error (0, 0, "%s: branch %s absent", rcs->path, rev);
	    return 1;
	}
    }
    else
	/* REV is an exact revision number. */
	xrev = xstrdup (rev);

    lock = findnode (RCS_getlocks (rcs), xrev);
    if (lock == NULL)
    {
	/* This revision isn't locked. */
	free (xrev);
	return 0;
    }

    if (strcmp (lock->data, user) != 0)
    {
        /* If the revision is locked by someone else, notify
	   them.  Note that this shouldn't ever happen if RCS_unlock
	   is called with a NULL revision, since that means "whatever
	   revision is currently locked by the caller." */
	char *repos, *workfile;
	repos = xstrdup (rcs->path);
	workfile = strrchr (repos, '/');
	*workfile++ = '\0';
	notify_do ('C', workfile, user, NULL, NULL, repos);
	free (repos);
    }

    delnode (lock);
    if (!unlock_quiet)
    {
	cvs_output (xrev, 0);
	cvs_output (" unlocked\n", 0);
    }

    free (xrev);
    return 0;
}

/* Add USER to the access list of RCS.  Do nothing if already present.
   FIXME-twp: check syntax of USER to make sure it's a valid id. */

void
RCS_addaccess (rcs, user)
    RCSNode *rcs;
    char *user;
{
    char *access, *a;

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    if (rcs->access == NULL)
	rcs->access = xstrdup (user);
    else
    {
	access = xstrdup (rcs->access);
	for (a = strtok (access, " "); a != NULL; a = strtok (NULL, " "))
	{
	    if (strcmp (a, user) == 0)
	    {
		free (access);
		return;
	    }
	}
	rcs->access = (char *) xrealloc
	    (rcs->access, strlen (rcs->access) + strlen (user) + 2);
	strcat (rcs->access, " ");
	strcat (rcs->access, user);
    }
}

/* Remove USER from the access list of RCS. */

void
RCS_delaccess (rcs, user)
    RCSNode *rcs;
    char *user;
{
    char *p, *s;
    int ulen;

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    if (rcs->access == NULL)
	return;

    p = rcs->access;
    ulen = strlen (user);
    while (p != NULL)
    {
	if (p[ulen] == '\0' || p[ulen] == ' ')
	    if (strncmp (p, user, ulen) == 0)
		break;
	p = strchr (p, ' ');
	if (p != NULL)
	    ++p;
    }

    if (p == NULL)
	return;

    s = p + ulen;
    while (*s != '\0')
	*p++ = *s++;
    *p = '\0';
}

char *
RCS_getaccess (rcs)
    RCSNode *rcs;
{
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    return rcs->access;
}

static int findtag PROTO ((Node *, void *));

/* Return a nonzero value if the revision specified by ARG is found.  */

static int
findtag (node, arg)
    Node *node;
    void *arg;
{
    char *rev = (char *)arg;

    if (strcmp (node->data, rev) == 0)
	return 1;
    else
	return 0;
}

/* Delete revisions between REV1 and REV2.  The changes between the two
   revisions must be collapsed, and the result stored in the revision
   immediately preceding the lower one.  Return 0 for successful completion,
   1 otherwise.

   Solution: check out the revision preceding REV1 and the revision
   following REV2.  Use call_diff to find aggregate diffs between
   these two revisions, and replace the delta text for the latter one
   with the new aggregate diff.  Alternatively, we could write a
   function that takes two change texts and combines them to produce a
   new change text, without checking out any revs or calling diff.  It
   would be hairy, but so, so cool.

   If INCLUSIVE is set, then TAG1 and TAG2, if non-NULL, tell us to
   delete that revision as well (cvs admin -o tag1:tag2).  If clear,
   delete up to but not including that revision (cvs admin -o tag1::tag2).
   This does not affect TAG1 or TAG2 being NULL; the meaning of the start
   point in ::tag2 and :tag2 is the same and likewise for end points.  */

int
RCS_delete_revs (rcs, tag1, tag2, inclusive)
    RCSNode *rcs;
    char *tag1;
    char *tag2;
    int inclusive;
{
    char *next;
    Node *nodep;
    RCSVers *revp = NULL;
    RCSVers *beforep;
    int status, found;
    int save_noexec;

    char *branchpoint = NULL;
    char *rev1 = NULL;
    char *rev2 = NULL;
    int rev1_inclusive = inclusive;
    int rev2_inclusive = inclusive;
    char *before = NULL;
    char *after = NULL;
    char *beforefile = NULL;
    char *afterfile = NULL;
    char *outfile = NULL;

    if (tag1 == NULL && tag2 == NULL)
	return 0;

    /* Assume error status until everything is finished. */
    status = 1;

    /* Make sure both revisions exist. */
    if (tag1 != NULL)
    {
	rev1 = RCS_gettag (rcs, tag1, 1, NULL);
	if (rev1 == NULL || (nodep = findnode (rcs->versions, rev1)) == NULL)
	{
	    error (0, 0, "%s: Revision %s doesn't exist.", rcs->path, tag1);
	    goto delrev_done;
	}
    }
    if (tag2 != NULL)
    {
	rev2 = RCS_gettag (rcs, tag2, 1, NULL);
	if (rev2 == NULL || (nodep = findnode (rcs->versions, rev2)) == NULL)
	{
	    error (0, 0, "%s: Revision %s doesn't exist.", rcs->path, tag2);
	    goto delrev_done;
	}
    }

    /* If rev1 is on the trunk and rev2 is NULL, rev2 should be
       RCS->HEAD.  (*Not* RCS_head(rcs), which may return rcs->branch
       instead.)  We need to check this special case early, in order
       to make sure that rev1 and rev2 get ordered correctly. */
    if (rev2 == NULL && numdots (rev1) == 1)
    {
	rev2 = xstrdup (rcs->head);
	rev2_inclusive = 1;
    }

    if (rev2 == NULL)
	rev2_inclusive = 1;

    if (rev1 != NULL && rev2 != NULL)
    {
	/* A range consisting of a branch number means the latest revision
	   on that branch. */
	if (RCS_isbranch (rcs, rev1) && strcmp (rev1, rev2) == 0)
	    rev1 = rev2 = RCS_getbranch (rcs, rev1, 0);
	else
	{
	    /* Make sure REV1 and REV2 are ordered correctly (in the
	       same order as the next field).  For revisions on the
	       trunk, REV1 should be higher than REV2; for branches,
	       REV1 should be lower.  */
	    /* Shouldn't we just be giving an error in the case where
	       the user specifies the revisions in the wrong order
	       (that is, always swap on the trunk, never swap on a
	       branch, in the non-error cases)?  It is not at all
	       clear to me that users who specify -o 1.4:1.2 really
	       meant to type -o 1.2:1.4, and the out of order usage
	       has never been documented, either by cvs.texinfo or
	       rcs(1).  */
	    char *temp;
	    int temp_inclusive;
	    if (numdots (rev1) == 1)
	    {
		if (compare_revnums (rev1, rev2) <= 0)
		{
		    temp = rev2;
		    rev2 = rev1;
		    rev1 = temp;

		    temp_inclusive = rev2_inclusive;
		    rev2_inclusive = rev1_inclusive;
		    rev1_inclusive = temp_inclusive;
		}
	    }
	    else if (compare_revnums (rev1, rev2) > 0)
	    {
		temp = rev2;
		rev2 = rev1;
		rev1 = temp;

		temp_inclusive = rev2_inclusive;
		rev2_inclusive = rev1_inclusive;
		rev1_inclusive = temp_inclusive;
	    }
	}
    }

    /* Basically the same thing; make sure that the ordering is what we
       need.  */
    if (rev1 == NULL)
    {
	assert (rev2 != NULL);
	if (numdots (rev2) == 1)
	{
	    /* Swap rev1 and rev2.  */
	    int temp_inclusive;

	    rev1 = rev2;
	    rev2 = NULL;

	    temp_inclusive = rev2_inclusive;
	    rev2_inclusive = rev1_inclusive;
	    rev1_inclusive = temp_inclusive;
	}
    }

    /* Put the revision number preceding the first one to delete into
       BEFORE (where "preceding" means according to the next field).
       If the first revision to delete is the first revision on its
       branch (e.g. 1.3.2.1), BEFORE should be the node on the trunk
       at which the branch is rooted.  If the first revision to delete
       is the head revision of the trunk, set BEFORE to NULL.

       Note that because BEFORE may not be on the same branch as REV1,
       it is not very handy for navigating the revision tree.  It's
       most useful just for checking out the revision preceding REV1. */
    before = NULL;
    branchpoint = RCS_getbranchpoint (rcs, rev1 != NULL ? rev1 : rev2);
    if (rev1 == NULL)
    {
	rev1 = xstrdup (branchpoint);
	if (numdots (branchpoint) > 1)
	{
	    char *bp;
	    bp = strrchr (branchpoint, '.');
	    while (*--bp != '.')
		;
	    *bp = '\0';
	    /* Note that this is exclusive, always, because the inclusive
	       flag doesn't affect the meaning when rev1 == NULL.  */
	    before = xstrdup (branchpoint);
	    *bp = '.';
	}
    }
    else if (strcmp (rev1, branchpoint) != 0)
    {
	/* Walk deltas from BRANCHPOINT on, looking for REV1. */
	nodep = findnode (rcs->versions, branchpoint);
	revp = (RCSVers *) nodep->data;
	while (revp->next != NULL && strcmp (revp->next, rev1) != 0)
	{
	    revp = (RCSVers *) nodep->data;
	    nodep = findnode (rcs->versions, revp->next);
	}
	if (revp->next == NULL)
	{
	    error (0, 0, "%s: Revision %s doesn't exist.", rcs->path, rev1);
	    goto delrev_done;
	}
	if (rev1_inclusive)
	    before = xstrdup (revp->version);
	else
	{
	    before = rev1;
	    nodep = findnode (rcs->versions, before);
	    rev1 = xstrdup (((RCSVers *)nodep->data)->next);
	}
    }
    else if (!rev1_inclusive)
    {
	before = rev1;
	nodep = findnode (rcs->versions, before);
	rev1 = xstrdup (((RCSVers *)nodep->data)->next);
    }
    else if (numdots (branchpoint) > 1)
    {
	/* Example: rev1 is "1.3.2.1", branchpoint is "1.3.2.1".
	   Set before to "1.3".  */
	char *bp;
	bp = strrchr (branchpoint, '.');
	while (*--bp != '.')
	    ;
	*bp = '\0';
	before = xstrdup (branchpoint);
	*bp = '.';
    }

    /* If any revision between REV1 and REV2 is locked or is a branch point,
       we can't delete that revision and must abort. */
    after = NULL;
    next = rev1;
    found = 0;
    while (!found && next != NULL)
    {
	nodep = findnode (rcs->versions, next);
	revp = (RCSVers *) nodep->data;

	if (rev2 != NULL)
	    found = (strcmp (revp->version, rev2) == 0);
	next = revp->next;

	if ((!found && next != NULL) || rev2_inclusive || rev2 == NULL)
	{
	    if (findnode (RCS_getlocks (rcs), revp->version))
	    {
		error (0, 0, "%s: can't remove locked revision %s",
		       rcs->path,
		       revp->version);
		goto delrev_done;
	    }
	    if (revp->branches != NULL)
	    {
		error (0, 0, "%s: can't remove branch point %s",
		       rcs->path,
		       revp->version);
		goto delrev_done;
	    }

	    /* Doing this only for the :: syntax is for compatibility.
	       See cvs.texinfo for somewhat more discussion.  */
	    if (!inclusive
		&& walklist (RCS_symbols (rcs), findtag, revp->version))
	    {
		/* We don't print which file this happens to on the theory
		   that the caller will print the name of the file in a
		   more useful fashion (fullname not rcs->path).  */
		error (0, 0, "cannot remove revision %s because it has tags",
		       revp->version);
		goto delrev_done;
	    }

	    /* It's misleading to print the `deleting revision' output
	       here, since we may not actually delete these revisions.
	       But that's how RCS does it.  Bleah.  Someday this should be
	       moved to the point where the revs are actually marked for
	       deletion. -twp */
	    cvs_output ("deleting revision ", 0);
	    cvs_output (revp->version, 0);
	    cvs_output ("\n", 1);
	}
    }

    if (rev2 == NULL)
	;
    else if (found)
    {
	if (rev2_inclusive)
	    after = xstrdup (next);
	else
	    after = xstrdup (revp->version);
    }
    else if (!inclusive)
    {
	/* In the case of an empty range, for example 1.2::1.2 or
	   1.2::1.3, we want to just do nothing.  */
	status = 0;
	goto delrev_done;
    }
    else
    {
	/* This looks fishy in the cases where tag1 == NULL or tag2 == NULL.
	   Are those cases really impossible?  */
	assert (tag1 != NULL);
	assert (tag2 != NULL);

	error (0, 0, "%s: invalid revision range %s:%s", rcs->path,
	       tag1, tag2);
	goto delrev_done;
    }

    /* The conditionals at this point get really hairy.  Here is the
       general idea:

       IF before != NULL and after == NULL
         THEN don't check out any revisions, just delete them
       IF before == NULL and after != NULL
         THEN only check out after's revision, and use it for the new deltatext
       ELSE
         check out both revisions and diff -n them.  This could use
	 RCS_exec_rcsdiff with some changes, like being able
	 to suppress diagnostic messages and to direct output. */

    assert (before != NULL || after != NULL);

    if (after != NULL)
    {
	char *diffbuf;
	size_t bufsize, len;

	afterfile = cvs_temp_name();
	status = RCS_checkout (rcs, NULL, after, NULL, NULL, afterfile,
			       (RCSCHECKOUTPROC)0, NULL);
	if (status > 0)
	    goto delrev_done;

	else if (status < 0)
	{
	    error (0, errno,
		   "cannot check out revision %s of %s", after, rcs->path);
	    goto delrev_done;
	}

	if (before == NULL)
	{
	    /* We are deleting revisions from the head of the tree,
	       so must create a new head. */
	    diffbuf = NULL;
	    bufsize = 0;
	    get_file (afterfile, afterfile, "r", &diffbuf, &bufsize, &len);

	    save_noexec = noexec;
	    noexec = 0;
	    if (unlink_file (afterfile) < 0)
		error (0, errno, "cannot remove %s", afterfile);
	    noexec = save_noexec;

	    free (afterfile);
	    afterfile = NULL;

	    free (rcs->head);
	    rcs->head = xstrdup (after);
	}
	else
	{
	    beforefile = cvs_temp_name();
	    status = RCS_checkout (rcs, NULL, before, NULL, NULL, beforefile,
				   (RCSCHECKOUTPROC)0, NULL);
	    if (status > 0)
		goto delrev_done;
	    else if (status < 0)
	    {
		error (0, errno, "cannot check out revision %s of %s",
		       before, rcs->path);
		goto delrev_done;
	    }

	    outfile = cvs_temp_name();
	    status = diff_exec (beforefile, afterfile, "-n", outfile);

	    if (status == 2)
	    {
		/* Not sure we need this message; will diff_exec already
		   have printed an error?  */
		error (0, 0, "%s: could not diff", rcs->path);
		status = 1;
		goto delrev_done;
	    }

	    diffbuf = NULL;
	    bufsize = 0;
	    get_file (outfile, outfile, "r", &diffbuf, &bufsize, &len);
	}

	/* Save the new change text in after's delta node. */
	nodep = findnode (rcs->versions, after);
	revp = (RCSVers *) nodep->data;

	assert (revp->text == NULL);

	revp->text = (Deltatext *) xmalloc (sizeof (Deltatext));
	memset ((Deltatext *) revp->text, 0, sizeof (Deltatext));
	revp->text->version = xstrdup (revp->version);
	revp->text->text = diffbuf;
	revp->text->len = len;

	/* If DIFFBUF is NULL, it means that OUTFILE is empty and that
	   there are no differences between the two revisions.  In that
	   case, we want to force RCS_copydeltas to write an empty string
	   for the new change text (leaving the text field set NULL
	   means "preserve the original change text for this delta," so
	   we don't want that). */
	if (revp->text->text == NULL)
	    revp->text->text = xstrdup ("");
    }

    /* Walk through the revisions (again) to mark each one as
       outdated.  (FIXME: would it be safe to use the `dead' field for
       this?  Doubtful.) */
    for (next = rev1;
	 next != NULL && (after == NULL || strcmp (next, after) != 0);
	 next = revp->next)
    {
	nodep = findnode (rcs->versions, next);
	revp = (RCSVers *) nodep->data;
	revp->outdated = 1;
    }

    /* Update delta links.  If BEFORE == NULL, we're changing the
       head of the tree and don't need to update any `next' links. */
    if (before != NULL)
    {
	/* If REV1 is the first node on its branch, then BEFORE is its
	   root node (on the trunk) and we have to update its branches
	   list.  Otherwise, BEFORE is on the same branch as AFTER, and
	   we can just change BEFORE's `next' field to point to AFTER.
	   (This should be safe: since findnode manages its lists via
	   the `hashnext' and `hashprev' fields, rather than `next' and
	   `prev', mucking with `next' and `prev' should not corrupt the
	   delta tree's internal structure.  Much. -twp) */

	if (rev1 == NULL)
	    /* beforep's ->next field already should be equal to after,
	       which I think is always NULL in this case.  */
	    ;
	else if (strcmp (rev1, branchpoint) == 0)
	{
	    nodep = findnode (rcs->versions, before);
	    revp = (RCSVers *) nodep->data;
	    nodep = revp->branches->list->next;
	    while (nodep != revp->branches->list &&
		   strcmp (nodep->key, rev1) != 0)
		nodep = nodep->next;
	    assert (nodep != revp->branches->list);
	    if (after == NULL)
		delnode (nodep);
	    else
	    {
		free (nodep->key);
		nodep->key = xstrdup (after);
	    }
	}
	else
	{
	    nodep = findnode (rcs->versions, before);
	    beforep = (RCSVers *) nodep->data;
	    free (beforep->next);
	    beforep->next = xstrdup (after);
	}
    }

    status = 0;

 delrev_done:
    if (rev1 != NULL)
	free (rev1);
    if (rev2 != NULL)
	free (rev2);
    if (branchpoint != NULL)
	free (branchpoint);
    if (before != NULL)
	free (before);
    if (after != NULL)
	free (after);

    save_noexec = noexec;
    noexec = 0;
    if (beforefile != NULL)
    {
	if (unlink_file (beforefile) < 0)
	    error (0, errno, "cannot remove %s", beforefile);
	free (beforefile);
    }
    if (afterfile != NULL)
    {
	if (unlink_file (afterfile) < 0)
	    error (0, errno, "cannot remove %s", afterfile);
	free (afterfile);
    }
    if (outfile != NULL)
    {
	if (unlink_file (outfile) < 0)
	    error (0, errno, "cannot remove %s", outfile);
	free (outfile);
    }
    noexec = save_noexec;

    return status;
}

/* RCS_deltas and friends.  Processing of the deltas in RCS files.  */

struct line
{
    /* Text of this line.  Part of the same malloc'd block as the struct
       line itself (we probably should use the "struct hack" (char text[1])
       and save ourselves sizeof (char *) bytes).  Does not include \n;
       instead has_newline indicates the presence or absence of \n.  */
    char *text;
    /* Length of this line, not counting \n if has_newline is true.  */
    size_t len;
    /* Version in which it was introduced.  */
    RCSVers *vers;
    /* Nonzero if this line ends with \n.  This will always be true
       except possibly for the last line.  */
    int has_newline;
    /* Number of pointers to this struct line.  */
    int refcount;
};

struct linevector
{
    /* How many lines in use for this linevector?  */
    unsigned int nlines;
    /* How many lines allocated for this linevector?  */
    unsigned int lines_alloced;
    /* Pointer to array containing a pointer to each line.  */
    struct line **vector;
};

static void linevector_init PROTO ((struct linevector *));

/* Initialize *VEC to be a linevector with no lines.  */
static void
linevector_init (vec)
    struct linevector *vec;
{
    vec->lines_alloced = 0;
    vec->nlines = 0;
    vec->vector = NULL;
}

static int linevector_add PROTO ((struct linevector *vec, const char *text,
				  size_t len, RCSVers *vers,
				  unsigned int pos));

/* Given some text TEXT, add each of its lines to VEC before line POS
   (where line 0 is the first line).  The last line in TEXT may or may
   not be \n terminated.
   Set the version for each of the new lines to VERS.  This
   function returns non-zero for success.  It returns zero if the line
   number is out of range.

   Each of the lines in TEXT are copied to space which is managed with
   the linevector (and freed by linevector_free).  So the caller doesn't
   need to keep TEXT around after the call to this function.  */
static int
linevector_add (vec, text, len, vers, pos)
    struct linevector *vec;
    const char *text;
    size_t len;
    RCSVers *vers;
    unsigned int pos;
{
    const char *textend;
    unsigned int i;
    unsigned int nnew;
    const char *p;
    const char *nextline_text;
    size_t nextline_len;
    int nextline_newline;
    struct line *q;

    if (len == 0)
	return 1;

    textend = text + len;

    /* Count the number of lines we will need to add.  */
    nnew = 1;
    for (p = text; p < textend; ++p)
	if (*p == '\n' && p + 1 < textend)
	    ++nnew;

    /* Expand VEC->VECTOR if needed.  */
    if (vec->nlines + nnew >= vec->lines_alloced)
    {
	if (vec->lines_alloced == 0)
	    vec->lines_alloced = 10;
	while (vec->nlines + nnew >= vec->lines_alloced)
	    vec->lines_alloced *= 2;
	vec->vector = xrealloc (vec->vector,
				vec->lines_alloced * sizeof (*vec->vector));
    }

    /* Make room for the new lines in VEC->VECTOR.  */
    for (i = vec->nlines + nnew - 1; i >= pos + nnew; --i)
	vec->vector[i] = vec->vector[i - nnew];

    if (pos > vec->nlines)
	return 0;

    /* Actually add the lines, to VEC->VECTOR.  */
    i = pos;
    nextline_text = text;
    nextline_newline = 0;
    for (p = text; p < textend; ++p)
	if (*p == '\n')
	{
	    nextline_newline = 1;
	    if (p + 1 == textend)
		/* If there are no characters beyond the last newline, we
		   don't consider it another line.  */
		break;
	    nextline_len = p - nextline_text;
	    q = (struct line *) xmalloc (sizeof (struct line) + nextline_len);
	    q->vers = vers;
	    q->text = (char *)q + sizeof (struct line);
	    q->len = nextline_len;
	    q->has_newline = nextline_newline;
	    q->refcount = 1;
	    memcpy (q->text, nextline_text, nextline_len);
	    vec->vector[i++] = q;

	    nextline_text = (char *)p + 1;
	    nextline_newline = 0;
	}
    nextline_len = p - nextline_text;
    q = (struct line *) xmalloc (sizeof (struct line) + nextline_len);
    q->vers = vers;
    q->text = (char *)q + sizeof (struct line);
    q->len = nextline_len;
    q->has_newline = nextline_newline;
    q->refcount = 1;
    memcpy (q->text, nextline_text, nextline_len);
    vec->vector[i] = q;

    vec->nlines += nnew;

    return 1;
}

static void linevector_delete PROTO ((struct linevector *, unsigned int,
				      unsigned int));

/* Remove NLINES lines from VEC at position POS (where line 0 is the
   first line).  */
static void
linevector_delete (vec, pos, nlines)
    struct linevector *vec;
    unsigned int pos;
    unsigned int nlines;
{
    unsigned int i;
    unsigned int last;

    last = vec->nlines - nlines;
    for (i = pos; i < pos + nlines; ++i)
    {
	if (--vec->vector[i]->refcount == 0)
	    free (vec->vector[i]);
    }
    for (i = pos; i < last; ++i)
	vec->vector[i] = vec->vector[i + nlines];
    vec->nlines -= nlines;
}

static void linevector_copy PROTO ((struct linevector *, struct linevector *));

/* Copy FROM to TO, copying the vectors but not the lines pointed to.  */
static void
linevector_copy (to, from)
    struct linevector *to;
    struct linevector *from;
{
    unsigned int ln;

    for (ln = 0; ln < to->nlines; ++ln)
    {
	if (--to->vector[ln]->refcount == 0)
	    free (to->vector[ln]);
    }
    if (from->nlines > to->lines_alloced)
    {
	if (to->lines_alloced == 0)
	    to->lines_alloced = 10;
	while (from->nlines > to->lines_alloced)
	    to->lines_alloced *= 2;
	to->vector = (struct line **)
	    xrealloc (to->vector, to->lines_alloced * sizeof (*to->vector));
    }
    memcpy (to->vector, from->vector,
	    from->nlines * sizeof (*to->vector));
    to->nlines = from->nlines;
    for (ln = 0; ln < to->nlines; ++ln)
	++to->vector[ln]->refcount;
}

static void linevector_free PROTO ((struct linevector *));

/* Free storage associated with linevector.  */
static void
linevector_free (vec)
    struct linevector *vec;
{
    unsigned int ln;

    if (vec->vector != NULL)
    {
	for (ln = 0; ln < vec->nlines; ++ln)
	    if (--vec->vector[ln]->refcount == 0)
		free (vec->vector[ln]);

	free (vec->vector);
    }
}

static char *month_printname PROTO ((char *));

/* Given a textual string giving the month (1-12), terminated with any
   character not recognized by atoi, return the 3 character name to
   print it with.  I do not think it is a good idea to change these
   strings based on the locale; they are standard abbreviations (for
   example in rfc822 mail messages) which should be widely understood.
   Returns a pointer into static readonly storage.  */
static char *
month_printname (month)
    char *month;
{
    static const char *const months[] =
      {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int mnum;

    mnum = atoi (month);
    if (mnum < 1 || mnum > 12)
	return "???";
    return (char *)months[mnum - 1];
}

static int
apply_rcs_changes PROTO ((struct linevector *, const char *, size_t,
			  const char *, RCSVers *, RCSVers *));

/* Apply changes to the line vector LINES.  DIFFBUF is a buffer of
   length DIFFLEN holding the change text from an RCS file (the output
   of diff -n).  NAME is used in error messages.  The VERS field of
   any line added is set to ADDVERS.  The VERS field of any line
   deleted is set to DELVERS, unless DELVERS is NULL, in which case
   the VERS field of deleted lines is unchanged.  The function returns
   non-zero if the change text is applied successfully.  It returns
   zero if the change text does not appear to apply to LINES (e.g., a
   line number is invalid).  If the change text is improperly
   formatted (e.g., it is not the output of diff -n), the function
   calls error with a status of 1, causing the program to exit.  */

static int
apply_rcs_changes (lines, diffbuf, difflen, name, addvers, delvers)
     struct linevector *lines;
     const char *diffbuf;
     size_t difflen;
     const char *name;
     RCSVers *addvers;
     RCSVers *delvers;
{
    const char *p;
    const char *q;
    int op;
    /* The RCS format throws us for a loop in that the deltafrags (if
       we define a deltafrag as an add or a delete) need to be applied
       in reverse order.  So we stick them into a linked list.  */
    struct deltafrag {
	enum {ADD, DELETE} type;
	unsigned long pos;
	unsigned long nlines;
	const char *new_lines;
	size_t len;
	struct deltafrag *next;
    };
    struct deltafrag *dfhead;
    struct deltafrag *df;

    dfhead = NULL;
    for (p = diffbuf; p != NULL && p < diffbuf + difflen; )
    {
	op = *p++;
	if (op != 'a' && op != 'd')
	    /* Can't just skip over the deltafrag, because the value
	       of op determines the syntax.  */
	    error (1, 0, "unrecognized operation '%c' in %s", op, name);
	df = (struct deltafrag *) xmalloc (sizeof (struct deltafrag));
	df->next = dfhead;
	dfhead = df;
	df->pos = strtoul (p, (char **) &q, 10);

	if (p == q)
	    error (1, 0, "number expected in %s", name);
	p = q;
	if (*p++ != ' ')
	    error (1, 0, "space expected in %s", name);
	df->nlines = strtoul (p, (char **) &q, 10);
	if (p == q)
	    error (1, 0, "number expected in %s", name);
	p = q;
	if (*p++ != '\012')
	    error (1, 0, "linefeed expected in %s", name);

	if (op == 'a')
	{
	    unsigned int i;

	    df->type = ADD;
	    i = df->nlines;
	    /* The text we want is the number of lines specified, or
	       until the end of the value, whichever comes first (it
	       will be the former except in the case where we are
	       adding a line which does not end in newline).  */
	    for (q = p; i != 0; ++q)
		if (*q == '\n')
		    --i;
		else if (q == diffbuf + difflen)
		{
		    if (i != 1)
			error (1, 0, "premature end of change in %s", name);
		    else
			break;
		}

	    /* Stash away a pointer to the text we are adding.  */
	    df->new_lines = p;
	    df->len = q - p;

	    p = q;
	}
	else
	{
	    /* Correct for the fact that line numbers in RCS files
	       start with 1.  */
	    --df->pos;

	    assert (op == 'd');
	    df->type = DELETE;
	}
    }

    for (df = dfhead; df != NULL;)
    {
	unsigned int ln;

	switch (df->type)
	{
	case ADD:
	    if (! linevector_add (lines, df->new_lines, df->len, addvers,
				  df->pos))
		return 0;
	    break;
	case DELETE:
	    if (df->pos > lines->nlines
		|| df->pos + df->nlines > lines->nlines)
		return 0;
	    if (delvers != NULL)
		for (ln = df->pos; ln < df->pos + df->nlines; ++ln)
		    lines->vector[ln]->vers = delvers;
	    linevector_delete (lines, df->pos, df->nlines);
	    break;
	}
	df = df->next;
	free (dfhead);
	dfhead = df;
    }

    return 1;
}

/* Apply an RCS change text to a buffer.  The function name starts
   with rcs rather than RCS because this does not take an RCSNode
   argument.  NAME is used in error messages.  TEXTBUF is the text
   buffer to change, and TEXTLEN is the size.  DIFFBUF and DIFFLEN are
   the change buffer and size.  The new buffer is returned in *RETBUF
   and *RETLEN.  The new buffer is allocated by xmalloc.

   Return 1 for success.  On failure, call error and return 0.  */

int
rcs_change_text (name, textbuf, textlen, diffbuf, difflen, retbuf, retlen)
     const char *name;
     char *textbuf;
     size_t textlen;
     const char *diffbuf;
     size_t difflen;
     char **retbuf;
     size_t *retlen;
{
    struct linevector lines;
    int ret;

    *retbuf = NULL;
    *retlen = 0;

    linevector_init (&lines);

    if (! linevector_add (&lines, textbuf, textlen, NULL, 0))
	error (1, 0, "cannot initialize line vector");

    if (! apply_rcs_changes (&lines, diffbuf, difflen, name, NULL, NULL))
    {
	error (0, 0, "invalid change text in %s", name);
	ret = 0;
    }
    else
    {
	char *p;
	size_t n;
	unsigned int ln;

	n = 0;
	for (ln = 0; ln < lines.nlines; ++ln)
	    /* 1 for \n */
	    n += lines.vector[ln]->len + 1;

	p = xmalloc (n);
	*retbuf = p;

	for (ln = 0; ln < lines.nlines; ++ln)
	{
	    memcpy (p, lines.vector[ln]->text, lines.vector[ln]->len);
	    p += lines.vector[ln]->len;
	    if (lines.vector[ln]->has_newline)
		*p++ = '\n';
	}

	*retlen = p - *retbuf;
	assert (*retlen <= n);

	ret = 1;
    }

    linevector_free (&lines);

    return ret;
}

/* Walk the deltas in RCS to get to revision VERSION.

   If OP is RCS_ANNOTATE, then write annotations using cvs_output.

   If OP is RCS_FETCH, then put the contents of VERSION into a
   newly-malloc'd array and put a pointer to it in *TEXT.  Each line
   is \n terminated; the caller is responsible for converting text
   files if desired.  The total length is put in *LEN.

   If FP is non-NULL, it should be a file descriptor open to the file
   RCS with file position pointing to the deltas.  We close the file
   when we are done.

   If LOG is non-NULL, then *LOG is set to the log message of VERSION,
   and *LOGLEN is set to the length of the log message.

   On error, give a fatal error.  */

static void
RCS_deltas (rcs, fp, version, op, text, len, log, loglen)
    RCSNode *rcs;
    FILE *fp;
    char *version;
    enum rcs_delta_op op;
    char **text;
    size_t *len;
    char **log;
    size_t *loglen;
{
    char *branchversion;
    char *cpversion;
    char *key;
    char *value;
    size_t vallen;
    RCSVers *vers;
    RCSVers *prev_vers;
    RCSVers *trunk_vers;
    char *next;
    int n;
    int ishead, isnext, isversion, onbranch;
    Node *node;
    struct linevector headlines;
    struct linevector curlines;
    struct linevector trunklines;
    int foundhead;

    if (fp == NULL)
    {
	fp = CVS_FOPEN (rcs->path, FOPEN_BINARY_READ);
	if (fp == NULL)
	    error (1, 0, "unable to reopen `%s'", rcs->path);
	if (fseek (fp, rcs->delta_pos, SEEK_SET) != 0)
	    error (1, 0, "cannot fseek RCS file");
    }

    ishead = 1;
    vers = NULL;
    prev_vers = NULL;
    trunk_vers = NULL;
    next = NULL;
    onbranch = 0;
    foundhead = 0;

    linevector_init (&curlines);
    linevector_init (&headlines);
    linevector_init (&trunklines);

    /* We set BRANCHVERSION to the version we are currently looking
       for.  Initially, this is the version on the trunk from which
       VERSION branches off.  If VERSION is not a branch, then
       BRANCHVERSION is just VERSION.  */
    branchversion = xstrdup (version);
    cpversion = strchr (branchversion, '.');
    if (cpversion != NULL)
        cpversion = strchr (cpversion + 1, '.');
    if (cpversion != NULL)
        *cpversion = '\0';

    do {
	getrcsrev (fp, &key);

	if (next != NULL && strcmp (next, key) != 0)
	{
	    /* This is not the next version we need.  It is a branch
               version which we want to ignore.  */
	    isnext = 0;
	    isversion = 0;
	}
	else
	{
	    isnext = 1;

	    /* look up the revision */
	    node = findnode (rcs->versions, key);
	    if (node == NULL)
	        error (1, 0,
		       "mismatch in rcs file %s between deltas and deltatexts",
		       rcs->path);

	    /* Stash the previous version.  */
	    prev_vers = vers;

	    vers = (RCSVers *) node->data;
	    next = vers->next;

	    /* Compare key and trunkversion now, because key points to
	       storage controlled by getrcskey.  */
	    if (strcmp (branchversion, key) == 0)
	        isversion = 1;
	    else
	        isversion = 0;
	}

	while ((n = getrcskey (fp, &key, &value, &vallen)) >= 0)
	{
	    if (log != NULL
		&& isversion
		&& strcmp (key, "log") == 0
		&& strcmp (branchversion, version) == 0)
	    {
		*log = xmalloc (vallen);
		memcpy (*log, value, vallen);
		*loglen = vallen;
	    }

	    if (strcmp (key, "text") == 0)
	    {
		if (ishead)
		{
		    if (! linevector_add (&curlines, value, vallen, NULL, 0))
			error (1, 0, "invalid rcs file %s", rcs->path);

		    ishead = 0;
		}
		else if (isnext)
		{
		    if (! apply_rcs_changes (&curlines, value, vallen,
					     rcs->path,
					     onbranch ? vers : NULL,
					     onbranch ? NULL : prev_vers))
			error (1, 0, "invalid change text in %s", rcs->path);
		}
		break;
	    }
	}
	if (n < 0)
	    goto l_error;

	if (isversion)
	{
	    /* This is either the version we want, or it is the
               branchpoint to the version we want.  */
	    if (strcmp (branchversion, version) == 0)
	    {
	        /* This is the version we want.  */
		linevector_copy (&headlines, &curlines);
		foundhead = 1;
		if (onbranch)
		{
		    /* We have found this version by tracking up a
                       branch.  Restore back to the lines we saved
                       when we left the trunk, and continue tracking
                       down the trunk.  */
		    onbranch = 0;
		    vers = trunk_vers;
		    next = vers->next;
		    linevector_copy (&curlines, &trunklines);
		}
	    }
	    else
	    {
	        Node *p;

	        /* We need to look up the branch.  */
	        onbranch = 1;

		if (numdots (branchversion) < 2)
		{
		    unsigned int ln;

		    /* We are leaving the trunk; save the current
                       lines so that we can restore them when we
                       continue tracking down the trunk.  */
		    trunk_vers = vers;
		    linevector_copy (&trunklines, &curlines);

		    /* Reset the version information we have
                       accumulated so far.  It only applies to the
                       changes from the head to this version.  */
		    for (ln = 0; ln < curlines.nlines; ++ln)
		        curlines.vector[ln]->vers = NULL;
		}

		/* The next version we want is the entry on
                   VERS->branches which matches this branch.  For
                   example, suppose VERSION is 1.21.4.3 and
                   BRANCHVERSION was 1.21.  Then we look for an entry
                   starting with "1.21.4" and we'll put it (probably
                   1.21.4.1) in NEXT.  We'll advance BRANCHVERSION by
                   two dots (in this example, to 1.21.4.3).  */

		if (vers->branches == NULL)
		    error (1, 0, "missing expected branches in %s",
			   rcs->path);
		*cpversion = '.';
		++cpversion;
		cpversion = strchr (cpversion, '.');
		if (cpversion == NULL)
		    error (1, 0, "version number confusion in %s",
			   rcs->path);
		for (p = vers->branches->list->next;
		     p != vers->branches->list;
		     p = p->next)
		    if (strncmp (p->key, branchversion,
				 cpversion - branchversion) == 0)
			break;
		if (p == vers->branches->list)
		    error (1, 0, "missing expected branch in %s",
			   rcs->path);

		next = p->key;

		cpversion = strchr (cpversion + 1, '.');
		if (cpversion != NULL)
		    *cpversion = '\0';
	    }
	}
	if (op == RCS_FETCH && foundhead)
	    break;
    } while (next != NULL);

    free (branchversion);
    
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", rcs->path);

    if (! foundhead)
        error (1, 0, "could not find desired version %s in %s",
	       version, rcs->path);

    /* Now print out or return the data we have just computed.  */
    switch (op)
    {
	case RCS_ANNOTATE:
	    {
		unsigned int ln;

		for (ln = 0; ln < headlines.nlines; ++ln)
		{
		    char buf[80];
		    /* Period which separates year from month in date.  */
		    char *ym;
		    /* Period which separates month from day in date.  */
		    char *md;
		    RCSVers *prvers;

		    prvers = headlines.vector[ln]->vers;
		    if (prvers == NULL)
			prvers = vers;

		    sprintf (buf, "%-12s (%-8.8s ",
			     prvers->version,
			     prvers->author);
		    cvs_output (buf, 0);

		    /* Now output the date.  */
		    ym = strchr (prvers->date, '.');
		    if (ym == NULL)
		    {
			/* ??- is an ANSI trigraph.  The ANSI way to
			   avoid it is \? but some pre ANSI compilers
			   complain about the unrecognized escape
			   sequence.  Of course string concatenation
			   ("??" "-???") is also an ANSI-ism.  Testing
			   __STDC__ seems to be a can of worms, since
			   compilers do all kinds of things with it.  */
			cvs_output ("??", 0);
			cvs_output ("-???", 0);
			cvs_output ("-??", 0);
		    }
		    else
		    {
			md = strchr (ym + 1, '.');
			if (md == NULL)
			    cvs_output ("??", 0);
			else
			    cvs_output (md + 1, 2);

			cvs_output ("-", 1);
			cvs_output (month_printname (ym + 1), 0);
			cvs_output ("-", 1);
			/* Only output the last two digits of the year.  Our output
			   lines are long enough as it is without printing the
			   century.  */
			cvs_output (ym - 2, 2);
		    }
		    cvs_output ("): ", 0);
		    if (headlines.vector[ln]->len != 0)
			cvs_output (headlines.vector[ln]->text,
				    headlines.vector[ln]->len);
		    cvs_output ("\n", 1);
		}
	    }
	    break;
	case RCS_FETCH:
	    {
		char *p;
		size_t n;
		unsigned int ln;

		assert (text != NULL);
		assert (len != NULL);

		n = 0;
		for (ln = 0; ln < headlines.nlines; ++ln)
		    /* 1 for \n */
		    n += headlines.vector[ln]->len + 1;
		p = xmalloc (n);
		*text = p;
		for (ln = 0; ln < headlines.nlines; ++ln)
		{
		    memcpy (p, headlines.vector[ln]->text,
			    headlines.vector[ln]->len);
		    p += headlines.vector[ln]->len;
		    if (headlines.vector[ln]->has_newline)
			*p++ = '\n';
		}
		*len = p - *text;
		assert (*len <= n);
	    }
	    break;
    }

    linevector_free (&curlines);
    linevector_free (&headlines);
    linevector_free (&trunklines);

    return;

  l_error:
    if (ferror (fp))
	error (1, errno, "cannot read %s", rcs->path);
    else
        error (1, 0, "%s does not appear to be a valid rcs file",
	       rcs->path);
}

static RCSVers *
getdelta (fp, rcsfile)
    FILE *fp;
    char *rcsfile;
{
    RCSVers *vnode;
    char *key, *value, *cp;
    long fpos;
    Node *kv;

    vnode = (RCSVers *) xmalloc (sizeof (RCSVers));
    memset (vnode, 0, sizeof (RCSVers));

    /* Get revision number. This uses getrcskey because it doesn't
       croak when encountering unexpected input.  As a result, we have
       to play unholy games with `key' and `value'. */
    fpos = ftell (fp);
    getrcskey (fp, &key, &value, NULL);

    /* Make sure that it is a revision number and not a cabbage 
       or something. */
    for (cp = key; (isdigit (*cp) || *cp == '.') && *cp != '\0'; cp++)
	/* do nothing */ ;
    if (*cp != '\0' || strncmp (RCSDATE, value, strlen (RCSDATE)) != 0)
    {
	(void) fseek (fp, fpos, SEEK_SET);
	free (vnode);
	return NULL;
    }
    vnode->version = xstrdup (key);

    /* grab the value of the date from value */
    cp = value + strlen (RCSDATE);/* skip the "date" keyword */
    while (whitespace (*cp))		/* take space off front of value */
	cp++;

    vnode->date = xstrdup (cp);

    /* Get author field.  */
    (void) getrcskey (fp, &key, &value, NULL);
    /* FIXME: should be using errno in case of ferror.  */
    if (key == NULL || strcmp (key, "author") != 0)
	error (1, 0, "\
unable to parse rcs file; `author' not in the expected place");
    vnode->author = xstrdup (value);

    /* Get state field.  */
    (void) getrcskey (fp, &key, &value, NULL);
    /* FIXME: should be using errno in case of ferror.  */
    if (key == NULL || strcmp (key, "state") != 0)
	error (1, 0, "\
unable to parse rcs file; `state' not in the expected place");
    vnode->state = xstrdup (value);
    if (strcmp (value, "dead") == 0)
    {
	vnode->dead = 1;
    }

    /* Note that "branches" and "next" are in fact mandatory, according
       to doc/RCSFILES.  We perhaps should be giving an error if they
       are not there.  */

    /* fill in the branch list (if any branches exist) */
    fpos = ftell (fp);
    (void) getrcskey (fp, &key, &value, NULL);
    /* FIXME: should be handling various error conditions better.  */
    if (key != NULL && strcmp (key, RCSDESC) == 0)
    {
	(void) fseek (fp, fpos, SEEK_SET);
	return vnode;
    }
    if (value != (char *) NULL)
    {
	vnode->branches = getlist ();
	do_branches (vnode->branches, value);
    }

    /* fill in the next field if there is a next revision */
    fpos = ftell (fp);
    (void) getrcskey (fp, &key, &value, NULL);
    /* FIXME: should be handling various error conditions better.  */
    if (key != NULL && strcmp (key, RCSDESC) == 0)
    {
	(void) fseek (fp, fpos, SEEK_SET);
	return vnode;
    }
    if (value != (char *) NULL)
	vnode->next = xstrdup (value);

    /*
     * XXX - this is where we put the symbolic link stuff???
     * (into newphrases in the deltas).
     */
    /* FIXME: Does not correctly handle errors, e.g. from stdio.  */
    while (1)
    {
	fpos = ftell (fp);
	if (getrcskey (fp, &key, &value, NULL) < 0)
	    break;

	assert (key != NULL);

	if (strcmp (key, RCSDESC) == 0)
	    break;

	/* Enable use of repositories created by certain obsolete
	   versions of CVS.  This code should remain indefinately;
	   there is no procedure for converting old repositories, and
	   checking for it is harmless.  */
	if (strcmp(key, RCSDEAD) == 0)
	{
	    vnode->dead = 1;
	    if (vnode->state != NULL)
		free (vnode->state);
	    vnode->state = xstrdup ("dead");
	    continue;
	}
	/* if we have a new revision number, we're done with this delta */
	for (cp = key; (isdigit (*cp) || *cp == '.') && *cp != '\0'; cp++)
	    /* do nothing */ ;
	if (*cp == '\0' && strncmp (RCSDATE, value, strlen (RCSDATE)) == 0)
	    break;

	/* At this point, key and value represent a user-defined field
	   in the delta node. */
	if (vnode->other_delta == NULL)
	    vnode->other_delta = getlist ();
	kv = getnode ();
	kv->type = RCSFIELD;
	kv->key = xstrdup (key);
	kv->data = xstrdup (value);
	if (addnode (vnode->other_delta, kv) != 0)
	{
	    /* Complaining about duplicate keys in newphrases seems
	       questionable, in that we don't know what they mean and
	       doc/RCSFILES has no prohibition on several newphrases
	       with the same key.  But we can't store more than one as
	       long as we store them in a List *.  */
	    error (0, 0, "warning: duplicate key `%s' in RCS file `%s'",
		   key, rcsfile);
	    freenode (kv);
	}
     }

    /* We got here because we read beyond the end of a delta.  Seek back
       to the beginning of the erroneous read. */
    (void) fseek (fp, fpos, SEEK_SET);

    return vnode;
}

static void
freedeltatext (d)
    Deltatext *d;
{
    if (d->version != NULL)
	free (d->version);
    if (d->log != NULL)
	free (d->log);
    if (d->text != NULL)
	free (d->text);
    if (d->other != (List *) NULL)
	dellist (&d->other);
    free (d);
}

static Deltatext *
RCS_getdeltatext (rcs, fp)
    RCSNode *rcs;
    FILE *fp;
{
    char *num;
    char *key, *value;
    int n;
    Node *p;
    Deltatext *d;
    size_t textlen;

    /* Get the revision number. */
    n = getrevnum (fp, &num);
    if (ferror (fp))
	error (1, errno, "%s: cannot read", rcs->path);
    if (n == EOF)
    {
	/* If n == EOF and num == NULL, it means we reached EOF
	   naturally.  That's fine. */
	if (num == NULL)
	    return NULL;
	else
	    error (1, 0, "%s: unexpected EOF", rcs->path);
    }

    p = findnode (rcs->versions, num);
    if (p == NULL)
	error (1, 0, "mismatch in rcs file %s between deltas and deltatexts",
	       rcs->path);

    d = (Deltatext *) xmalloc (sizeof (Deltatext));
    d->version = xstrdup (num);

    /* Get the log message. */
    if (getrcskey (fp, &key, &value, NULL) < 0)
	error (1, 0, "%s, delta %s: unexpected EOF", rcs->path, num);
    if (strcmp (key, "log") != 0)
	error (1, 0, "%s, delta %s: expected `log', got `%s'",
	       rcs->path, num, key);
    d->log = xstrdup (value);

    /* Get random newphrases. */
    d->other = getlist();
    for (n = getrcskey (fp, &key, &value, &textlen);
	 n >= 0 && strcmp (key, "text") != 0;
	 n = getrcskey (fp, &key, &value, &textlen))
    {
	p = getnode();
	p->type = RCSFIELD;
	p->key = xstrdup (key);
	p->data = xstrdup (value);
	if (addnode (d->other, p) < 0)
	{
	    error (0, 0, "warning: %s, delta %s: duplicate field `%s'",
		   rcs->path, num, key);
	}
    }
    if (n < 0)
	error (1, 0, "%s, delta %s: unexpected EOF", rcs->path, num);

    /* Get the change text. We already know that this key is `text'. */
    d->text = (char *) malloc (textlen + 1);
    d->len = textlen;
    memcpy (d->text, value, textlen);

    return d;
}

/* RCS output functions, for writing RCS format files from RCSNode
   structures.

   For most of this work, RCS 5.7 uses an `aprintf' function which aborts
   program upon error.  Instead, these functions check the output status
   of the stream right before closing it, and aborts if an error condition
   is found.  The RCS solution is probably the better one: it produces
   more overhead, but will produce a clearer diagnostic in the case of
   catastrophic error.  In either case, however, the repository will probably
   not get corrupted. */

static int
putsymbol_proc (symnode, fp)
    Node *symnode;
    void *fp;
{
    return fprintf ((FILE *) fp, "\n\t%s:%s", symnode->key, symnode->data);
}

static int putlock_proc PROTO ((Node *, void *));

/* putlock_proc is like putsymbol_proc, but key and data are reversed. */

static int
putlock_proc (symnode, fp)
    Node *symnode;
    void *fp;
{
    return fprintf ((FILE *) fp, "\n\t%s:%s", symnode->data, symnode->key);
}

static int
putrcsfield_proc (node, vfp)
    Node *node;
    void *vfp;
{
    FILE *fp = (FILE *) vfp;

    /* Some magic keys used internally by CVS start with `;'. Skip them. */
    if (node->key[0] == ';')
	return 0;

    fprintf (fp, "\n%s\t", node->key);
    if (node->data != NULL)
    {
	/* If the field's value contains evil characters,
	   it must be stringified. */
	/* FIXME: This does not quite get it right.  "7jk8f" is not a legal
	   value for a value in a newpharse, according to doc/RCSFILES,
	   because digits are not valid in an "id".  We might do OK by
	   always writing strings (enclosed in @@).  Would be nice to
	   explicitly mention this one way or another in doc/RCSFILES.
	   A case where we are wrong in a much more clear-cut way is that
	   we let through non-graphic characters such as whitespace and
	   control characters.  */
	int n = strcspn (node->data, "$,.:;@");
	if (node->data[n] == 0)
	    fputs (node->data, fp);
	else
	{
	    putc ('@', fp);
	    expand_at_signs (node->data, (off_t) strlen (node->data), fp);
	    putc ('@', fp);
	}
    }

    /* desc, log and text fields should not be terminated with semicolon;
       all other fields should be. */
    if (strcmp (node->key, "desc") != 0 &&
	strcmp (node->key, "log") != 0 &&
	strcmp (node->key, "text") != 0)
    {
	putc (';', fp);
    }
    return 0;
}

/* Output the admin node for RCS into stream FP. */

static void
RCS_putadmin (rcs, fp)
    RCSNode *rcs;
    FILE *fp;
{
    fprintf (fp, "%s\t%s;\n", RCSHEAD, rcs->head ? rcs->head : "");
    if (rcs->branch)
	fprintf (fp, "%s\t%s;\n", RCSBRANCH, rcs->branch);

    fputs ("access", fp);
    if (rcs->access)
    {
	char *p, *s;
	s = xstrdup (rcs->access);
	for (p = strtok (s, " \n\t"); p != NULL; p = strtok (NULL, " \n\t"))
	    fprintf (fp, "\n\t%s", p);
	free (s);
    }
    fputs (";\n", fp);

    fputs (RCSSYMBOLS, fp);
    walklist (RCS_symbols(rcs), putsymbol_proc, (void *) fp);
    fputs (";\n", fp);

    fputs ("locks", fp);
    if (rcs->locks_data)
	fprintf (fp, "\t%s", rcs->locks_data);
    else if (rcs->locks)
	walklist (rcs->locks, putlock_proc, (void *) fp);
    if (rcs->strict_locks)
	fprintf (fp, "; strict");
    fputs (";\n", fp);

    if (rcs->comment)
    {
	fprintf (fp, "comment\t@");
	expand_at_signs (rcs->comment, (off_t) strlen (rcs->comment), fp);
	fputs ("@;\n", fp);
    }
    if (rcs->expand && strcmp (rcs->expand, "kv") != 0)
	fprintf (fp, "%s\t@%s@;\n", RCSEXPAND, rcs->expand);

    walklist (rcs->other, putrcsfield_proc, (void *) fp);

    putc ('\n', fp);
}

static void
putdelta (vers, fp)
    RCSVers *vers;
    FILE *fp;
{
    Node *bp, *start;

    /* Skip if no revision was supplied, or if it is outdated (cvs admin -o) */
    if (vers == NULL || vers->outdated)
	return;

    fprintf (fp, "\n%s\n%s\t%s;\t%s %s;\t%s %s;\nbranches",
	     vers->version,
	     RCSDATE, vers->date,
	     "author", vers->author,
	     "state", vers->state ? vers->state : "");

    if (vers->branches != NULL)
    {
	start = vers->branches->list;
	for (bp = start->next; bp != start; bp = bp->next)
	    fprintf (fp, "\n\t%s", bp->key);
    }

    fprintf (fp, ";\nnext\t%s;", vers->next ? vers->next : "");

    walklist (vers->other_delta, putrcsfield_proc, fp);

    putc ('\n', fp);
}

static void
RCS_putdtree (rcs, rev, fp)
    RCSNode *rcs;
    char *rev;
    FILE *fp;
{
    RCSVers *versp;
    Node *p, *branch;

    if (rev == NULL)
	return;

    /* Find the delta node for this revision. */
    p = findnode (rcs->versions, rev);
    assert (p != NULL);
    versp = (RCSVers *) p->data;

    /* Print the delta node and recurse on its `next' node.  This prints
       the trunk.  If there are any branches printed on this revision,
       print those trunks as well. */
    putdelta (versp, fp);
    RCS_putdtree (rcs, versp->next, fp);
    if (versp->branches != NULL)
    {
	branch = versp->branches->list;
	for (p = branch->next; p != branch; p = p->next)
	    RCS_putdtree (rcs, p->key, fp);
    }
}

static void
RCS_putdesc (rcs, fp)
    RCSNode *rcs;
    FILE *fp;
{
    fprintf (fp, "\n\n%s\n@", RCSDESC);
    if (rcs->desc != NULL)
    {
	off_t len = (off_t) strlen (rcs->desc);
	if (len > 0)
	{
	    expand_at_signs (rcs->desc, len, fp);
	    if (rcs->desc[len-1] != '\n')
		putc ('\n', fp);
	}
    }
    fputs ("@\n", fp);
}

static void
putdeltatext (fp, d)
    FILE *fp;
    Deltatext *d;
{
    fprintf (fp, "\n\n%s\nlog\n@", d->version);
    if (d->log != NULL)
    {
	int loglen = strlen (d->log);
	expand_at_signs (d->log, (off_t) loglen, fp);
	if (d->log[loglen-1] != '\n')
	    putc ('\n', fp);
    }
    putc ('@', fp);

    walklist (d->other, putrcsfield_proc, fp);

    fputs ("\ntext\n@", fp);
    if (d->text != NULL)
	expand_at_signs (d->text, (off_t) d->len, fp);
    fputs ("@\n", fp);
}

/* TODO: the whole mechanism for updating deltas is kludgey... more
   sensible would be to supply all the necessary info in a `newdeltatext'
   field for RCSVers nodes. -twp */

/* Copy delta text nodes from FIN to FOUT.  If NEWDTEXT is non-NULL, it
   is a new delta text node, and should be added to the tree at the
   node whose revision number is INSERTPT.  (Note that trunk nodes are
   written in decreasing order, and branch nodes are written in
   increasing order.) */

static void
RCS_copydeltas (rcs, fin, fout, newdtext, insertpt)
    RCSNode *rcs;
    FILE *fin;
    FILE *fout;
    Deltatext *newdtext;
    char *insertpt;
{
    Deltatext *dtext;
    RCSVers *dadmin;
    Node *np;
    int insertbefore, found;

    /* Make a note of whether NEWDTEXT should be inserted
       before or after its INSERTPT. */
    insertbefore = (newdtext != NULL && numdots (newdtext->version) == 1);

    found = 0;
    while ((dtext = RCS_getdeltatext (rcs, fin)) != NULL)
    {
	found = (insertpt != NULL && strcmp (dtext->version, insertpt) == 0);
	if (found && insertbefore)
	    putdeltatext (fout, newdtext);

	np = findnode (rcs->versions, dtext->version);
	dadmin = (RCSVers *) np->data;

	/* If this revision has been outdated, just skip it. */
	if (dadmin->outdated)
	    continue;
	   
	/* Update the change text for this delta.  New change text
	   data may come from cvs admin -m, cvs admin -o, or cvs ci. */
	if (dadmin->text != NULL)
	{
	    if (dadmin->text->log != NULL)
	    {
		free (dtext->log);
		dtext->log = dadmin->text->log;
		dadmin->text->log = NULL;
	    }
	    if (dadmin->text->text != NULL)
	    {
		free (dtext->text);
		dtext->text = dadmin->text->text;
		dtext->len = dadmin->text->len;
		dadmin->text->text = NULL;
	    }
	}
	putdeltatext (fout, dtext);
	freedeltatext (dtext);

	if (found && !insertbefore)
	    putdeltatext (fout, newdtext);
    }
    putc ('\n', fout);
}

/* RCS_internal_lockfile and RCS_internal_unlockfile perform RCS-style
   locking on the specified RCSFILE: for a file called `foo,v', open
   for writing a file called `,foo,'.

   Note that we what do here is quite different from what RCS does.
   RCS creates the ,foo, file before it reads the RCS file (if it
   knows that it will be writing later), so that it actually serves as
   a lock.  We don't; instead we rely on CVS writelocks.  This means
   that if someone is running RCS on the file at the same time they
   are running CVS on it, they might lose (we read the file,
   then RCS writes it, then we write it, clobbering the
   changes made by RCS).  I believe the current sentiment about this
   is "well, don't do that".

   A concern has been expressed about whether adopting the RCS
   strategy would slow us down.  I don't think so, since we need to
   write the ,foo, file anyway (unless perhaps if O_EXCL is slower or
   something).

   These do not perform quite the same function as the RCS -l option
   for locking files: they are intended to prevent competing RCS
   processes from stomping all over each other's laundry.  Hence,
   they are `internal' locking functions.

   Note that we don't clean up the ,foo, file on ^C.  We probably should.
   I'm not completely sure whether RCS does or not (I looked at the code
   a little, and didn't find it).

   If there is an error, give a fatal error; if we return we always
   return a non-NULL value.  */

static FILE *
rcs_internal_lockfile (rcsfile)
    char *rcsfile;
{
    char *lockfile;
    int fd;
    struct stat rstat;
    FILE *fp;

    /* Get the lock file name: `,file,' for RCS file `file,v'. */
    lockfile = rcs_lockfilename (rcsfile);

    /* Use the existing RCS file mode, or read-only if this is a new
       file.  (Really, this is a lie -- if this is a new file,
       RCS_checkin uses the permissions from the working copy.  For
       actually creating the file, we use 0444 as a safe default mode.) */
    if (stat (rcsfile, &rstat) < 0)
    {
	if (existence_error (errno))
	    rstat.st_mode = S_IRUSR | S_IRGRP | S_IROTH;
	else
	    error (1, errno, "cannot stat %s", rcsfile);
    }

    /* Try to open exclusively.  POSIX.1 guarantees that O_EXCL|O_CREAT
       guarantees an exclusive open.  According to the RCS source, with
       NFS v2 we must also throw in O_TRUNC and use an open mask that makes
       the file unwriteable.  For extensive justification, see the comments for
       rcswriteopen() in rcsedit.c, in RCS 5.7.  This is kind of pointless
       in the CVS case; see comment at the start of this file concerning
       general ,foo, file strategy.

       There is some sentiment that with NFSv3 and such, that one can
       rely on O_EXCL these days.  This might be true for unix (I
       don't really know), but I am still pretty skeptical in the case
       of the non-unix systems.  */
    fd = open (lockfile, OPEN_BINARY | O_WRONLY | O_CREAT | O_EXCL | O_TRUNC,
	       S_IRUSR | S_IRGRP | S_IROTH);

    if (fd < 0)
    {
	error (1, errno, "could not open lock file `%s'", lockfile);
    }

    free (lockfile);

    /* Force the file permissions, and return a stream object. */
    /* Because we change the modes later, we don't worry about
       this in the non-HAVE_FCHMOD case.  */
#ifdef HAVE_FCHMOD
    if (fchmod (fd, rstat.st_mode) < 0)
	error (1, errno, "cannot change mode for %s", lockfile);
#endif
    fp = fdopen (fd, FOPEN_BINARY_WRITE);
    if (fp == NULL)
	error (1, errno, "cannot fdopen %s", lockfile);
    return fp;
}

static void
rcs_internal_unlockfile (fp, rcsfile)
    FILE *fp;
    char *rcsfile;
{
    char *lockfile;

    /* Get the lock file name: `,file,' for RCS file `file,v'. */
    lockfile = rcs_lockfilename (rcsfile);

    /* Abort if we could not write everything successfully to LOCKFILE.
       This is not a great error-handling mechanism, but should prevent
       corrupting the repository. */

    if (ferror (fp))
	error (1, 0, "error writing to lock file %s", lockfile);
    if (fclose (fp) == EOF)
	error (1, errno, "error closing lock file %s", lockfile);

    rename_file (lockfile, rcsfile);
    free (lockfile);
}

static char *
rcs_lockfilename (rcsfile)
    char *rcsfile;
{
    char *lockfile, *lockp;
    char *rcsbase, *rcsp, *rcsend;
    int rcslen;

    /* Create the lockfile name. */
    rcslen = strlen (rcsfile);
    lockfile = (char *) xmalloc (rcslen + 10);
    rcsbase = last_component (rcsfile);
    rcsend = rcsfile + rcslen - sizeof(RCSEXT);
    for (lockp = lockfile, rcsp = rcsfile; rcsp < rcsbase; ++rcsp)
	*lockp++ = *rcsp;
    *lockp++ = ',';
    while (rcsp <= rcsend)
	*lockp++ = *rcsp++;
    *lockp++ = ',';
    *lockp = '\0';

    return lockfile;
}

/* Rewrite an RCS file.  The basic idea here is that the caller should
   first call RCS_reparsercsfile, then munge the data structures as
   desired (via RCS_delete_revs, RCS_settag, &c), then call RCS_rewrite.  */

void
RCS_rewrite (rcs, newdtext, insertpt)
    RCSNode *rcs;
    Deltatext *newdtext;
    char *insertpt;
{
    FILE *fin, *fout;

    if (noexec)
	return;

    fout = rcs_internal_lockfile (rcs->path);

    RCS_putadmin (rcs, fout);
    RCS_putdtree (rcs, rcs->head, fout);
    RCS_putdesc (rcs, fout);

    /* Open the original RCS file and seek to the first delta text. */
    if ((fin = CVS_FOPEN (rcs->path, FOPEN_BINARY_READ)) == NULL) 
	error (1, errno, "cannot open RCS file `%s' for reading", rcs->path);
    if (fseek (fin, rcs->delta_pos, SEEK_SET) < 0)
	error (1, errno, "cannot fseek in RCS file %s", rcs->path);

    /* Update delta_pos to the current position in the output file.
       Do NOT move these statements: they must be done after fin has
       been positioned at the old delta_pos, but before any delta
       texts have been written to fout. */
    rcs->delta_pos = ftell (fout);
    if (rcs->delta_pos == -1)
	error (1, errno, "cannot ftell in RCS file %s", rcs->path);

    RCS_copydeltas (rcs, fin, fout, newdtext, insertpt);

    if (ferror (fin))
	error (0, errno, "warning: when closing RCS file `%s'", rcs->path);
    if (fclose (fin) < 0)
	error (0, errno, "warning: closing RCS file `%s'", rcs->path);

    rcs_internal_unlockfile (fout, rcs->path);
}


/* Annotate command.  In rcs.c for historical reasons (from back when
   what is now RCS_deltas was part of annotate_fileproc).  */

/* Options from the command line.  */

static int force_tag_match = 1;
static char *tag = NULL;
static char *date = NULL;

static int annotate_fileproc PROTO ((void *callerdat, struct file_info *));

static int
annotate_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    FILE *fp = NULL;
    char *version;

    if (finfo->rcs == NULL)
        return (1);

    if (finfo->rcs->flags & PARTIAL)
        RCS_reparsercsfile (finfo->rcs, &fp);

    version = RCS_getversion (finfo->rcs, tag, date, force_tag_match,
			      (int *) NULL);
    if (version == NULL)
        return 0;

    /* Distinguish output for various files if we are processing
       several files.  */
    cvs_outerr ("Annotations for ", 0);
    cvs_outerr (finfo->fullname, 0);
    cvs_outerr ("\n***************\n", 0);

    RCS_deltas (finfo->rcs, fp, version, RCS_ANNOTATE, (char **) NULL,
		(size_t) NULL, (char **) NULL, (size_t *) NULL);
    free (version);
    return 0;
}

static const char *const annotate_usage[] =
{
    "Usage: %s %s [-lRf] [-r rev|-D date] [files...]\n",
    "\t-l\tLocal directory only, no recursion.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-f\tUse head revision if tag/date not found.\n",
    "\t-r rev\tAnnotate file as of specified revision/tag.\n",
    "\t-D date\tAnnotate file as of specified date.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* Command to show the revision, date, and author where each line of a
   file was modified.  */

int
annotate (argc, argv)
    int argc;
    char **argv;
{
    int local = 0;
    int c;

    if (argc == -1)
	usage (annotate_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+lr:D:fR")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'r':
	        tag = optarg;
		break;
	    case 'D':
	        date = Make_Date (optarg);
		break;
	    case 'f':
	        force_tag_match = 0;
		break;
	    case '?':
	    default:
		usage (annotate_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	start_server ();
	ign_setup ();

	if (local)
	    send_arg ("-l");
	if (!force_tag_match)
	    send_arg ("-f");
	option_with_arg ("-r", tag);
	if (date)
	    client_senddate (date);
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
	send_to_server ("annotate\012", 0);
	return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    return start_recursion (annotate_fileproc, (FILESDONEPROC) NULL,
			    (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			    argc, argv, local, W_LOCAL, 0, 1, (char *)NULL,
			    1);
}

/*
 * For a given file with full pathname PATH and revision number REV,
 * produce a file label suitable for passing to diff.  The default
 * file label as used by RCS 5.7 looks like this:
 *
 *	FILENAME <tab> YYYY/MM/DD <sp> HH:MM:SS <tab> REVNUM
 *
 * The date and time used are the revision's last checkin date and time.
 * If REV is NULL, use the working copy's mtime instead.
 */
char *
make_file_label (path, rev, rcs)
    char *path;
    char *rev;
    RCSNode *rcs;
{
    char datebuf[MAXDATELEN];
    char *label;
    char *file;

    file = last_component (path);
    label = (char *) xmalloc (strlen (file)
			      + (rev == NULL ? 0 : strlen (rev))
			      + 50);

    if (rev)
    {
	char *date;
	RCS_getrevtime (rcs, rev, datebuf, 0);
	date = printable_date (datebuf);
	(void) sprintf (label, "-L%s\t%s\t%s", file, date, rev);
	free (date);
    }
    else
    {
	struct stat sb;
	struct tm *wm;

	if (CVS_STAT (file, &sb) < 0)
	    error (0, 1, "could not get info for `%s'", path);
	else
	{
	    wm = gmtime (&sb.st_mtime);
	    (void) sprintf (datebuf, "%04d/%02d/%02d %02d:%02d:%02d",
			    wm->tm_year + 1900, wm->tm_mon + 1,
			    wm->tm_mday, wm->tm_hour,
			    wm->tm_min, wm->tm_sec);
	    (void) sprintf (label, "-L%s\t%s", file, datebuf);
	}
    }
    return label;
}

