/*	$OpenBSD: dir.c,v 1.22 2000/06/23 16:15:49 espie Exp $	*/
/*	$NetBSD: dir.c,v 1.14 1997/03/29 16:51:26 christos Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)dir.c	8.2 (Berkeley) 1/2/94";
#else
static char rcsid[] = "$OpenBSD: dir.c,v 1.22 2000/06/23 16:15:49 espie Exp $";
#endif
#endif /* not lint */

/*-
 * dir.c --
 *	Directory searching using wildcards and/or normal names...
 *	Used both for source wildcarding in the Makefile and for finding
 *	implicit sources.
 *
 * The interface for this module is:
 *	Dir_Init  	    Initialize the module.
 *
 *	Dir_End  	    Cleanup the module.
 *
 *	Dir_HasWildcards    Returns TRUE if the name given it needs to
 *	    	  	    be wildcard-expanded.
 *
 *	Dir_Expand	    Given a pattern and a path, return a Lst of names
 *	    	  	    which match the pattern on the search path.
 *
 *	Dir_FindFile	    Searches for a file on a given search path.
 *	    	  	    If it exists, the entire path is returned.
 *	    	  	    Otherwise NULL is returned.
 *
 *	Dir_MTime 	    Return TRUE if node exists. The file
 *	    	  	    is searched for along the default search path.
 *	    	  	    The path and mtime fields of the node are filled
 *	    	  	    in.
 *
 *	Dir_AddDir	    Add a directory to a search path.
 *
 *	Dir_MakeFlags	    Given a search path and a command flag, create
 *	    	  	    a string with each of the directories in the path
 *	    	  	    preceded by the command flag and all of them
 *	    	  	    separated by a space.
 *
 *	Dir_Destroy	    Destroy an element of a search path. Frees up all
 *	    	  	    things that can be freed for the element as long
 *	    	  	    as the element is no longer referenced by any other
 *	    	  	    search path.
 *	Dir_ClearPath	    Resets a search path to the empty list.
 *
 * For debugging:
 *	Dir_PrintDirectories	Print stats about the directory cache.
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include "make.h"
#include "hash.h"
#include "dir.h"

/*
 *	A search path consists of a Lst of Path structures. A Path structure
 *	has in it the name of the directory and a hash table of all the files
 *	in the directory. This is used to cut down on the number of system
 *	calls necessary to find implicit dependents and their like. Since
 *	these searches are made before any actions are taken, we need not
 *	worry about the directory changing due to creation commands. If this
 *	hampers the style of some makefiles, they must be changed.
 *
 *	A list of all previously-read directories is kept in the
 *	openDirectories Lst. This list is checked first before a directory
 *	is opened.
 *
 *	The need for the caching of whole directories is brought about by
 *	the multi-level transformation code in suff.c, which tends to search
 *	for far more files than regular make does. In the initial
 *	implementation, the amount of time spent performing "stat" calls was
 *	truly astronomical. The problem with hashing at the start is,
 *	of course, that pmake doesn't then detect changes to these directories
 *	during the course of the make. Three possibilities suggest themselves:
 *
 *	    1) just use stat to test for a file's existence. As mentioned
 *	       above, this is very inefficient due to the number of checks
 *	       engendered by the multi-level transformation code.
 *	    2) use readdir() and company to search the directories, keeping
 *	       them open between checks. I have tried this and while it
 *	       didn't slow down the process too much, it could severely
 *	       affect the amount of parallelism available as each directory
 *	       open would take another file descriptor out of play for
 *	       handling I/O for another job. Given that it is only recently
 *	       that UNIX OS's have taken to allowing more than 20 or 32
 *	       file descriptors for a process, this doesn't seem acceptable
 *	       to me.
 *	    3) record the mtime of the directory in the Path structure and
 *	       verify the directory hasn't changed since the contents were
 *	       hashed. This will catch the creation or deletion of files,
 *	       but not the updating of files. However, since it is the
 *	       creation and deletion that is the problem, this could be
 *	       a good thing to do. Unfortunately, if the directory (say ".")
 *	       were fairly large and changed fairly frequently, the constant
 *	       rehashing could seriously degrade performance. It might be
 *	       good in such cases to keep track of the number of rehashes
 *	       and if the number goes over a (small) limit, resort to using
 *	       stat in its place.
 *
 *	An additional thing to consider is that pmake is used primarily
 *	to create C programs and until recently pcc-based compilers refused
 *	to allow you to specify where the resulting object file should be
 *	placed. This forced all objects to be created in the current
 *	directory. This isn't meant as a full excuse, just an explanation of
 *	some of the reasons for the caching used here.
 *
 *	One more note: the location of a target's file is only performed
 *	on the downward traversal of the graph and then only for terminal
 *	nodes in the graph. This could be construed as wrong in some cases,
 *	but prevents inadvertent modification of files when the "installed"
 *	directory for a file is provided in the search path.
 *
 *	Another data structure maintained by this module is an mtime
 *	cache used when the searching of cached directories fails to find
 *	a file. In the past, Dir_FindFile would simply perform an access()
 *	call in such a case to determine if the file could be found using
 *	just the name given. When this hit, however, all that was gained
 *	was the knowledge that the file existed. Given that an access() is
 *	essentially a stat() without the copyout() call, and that the same
 *	filesystem overhead would have to be incurred in Dir_MTime, it made
 *	sense to replace the access() with a stat() and record the mtime
 *	in a cache for when Dir_MTime was actually called.
 */

LIST          dirSearchPath;	/* main search path */

static LIST   openDirectories;	/* the list of all open directories */

/*
 * Variables for gathering statistics on the efficiency of the hashing
 * mechanism.
 */
static int    hits,	      /* Found in directory cache */
	      misses,	      /* Sad, but not evil misses */
	      nearmisses,     /* Found under search path */
	      bigmisses;      /* Sought by itself */

static Path    	  *dot;	    /* contents of current directory */
static Hash_Table mtimes;   /* Results of doing a last-resort stat in
			     * Dir_FindFile -- if we have to go to the
			     * system to find the file, we might as well
			     * have its mtime on record. XXX: If this is done
			     * way early, there's a chance other rules will
			     * have already updated the file, in which case
			     * we'll update it again. Generally, there won't
			     * be two rules to update a single file, so this
			     * should be ok, but... */


static int DirFindName __P((void *, void *));
static int DirMatchFiles __P((char *, Path *, Lst));
static void DirExpandCurly __P((char *, char *, Lst, Lst));
static void DirExpandInt __P((char *, Lst, Lst));
static void DirPrintWord __P((void *));
static void DirPrintDir __P((void *));

/*-
 *-----------------------------------------------------------------------
 * Dir_Init --
 *	initialize things for this module
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	some directories may be opened.
 *-----------------------------------------------------------------------
 */
void
Dir_Init()
{
    Lst_Init(&dirSearchPath);
    Lst_Init(&openDirectories);
    Hash_InitTable(&mtimes, 0);

    /*
     * Since the Path structure is placed on both openDirectories and
     * the path we give Dir_AddDir (which in this case is openDirectories),
     * we need to remove "." from openDirectories and what better time to
     * do it than when we have to fetch the thing anyway?
     */
    Dir_AddDir(&openDirectories, ".");
    dot = (Path *)Lst_DeQueue(&openDirectories);

    /*
     * We always need to have dot around, so we increment its reference count
     * to make sure it's not destroyed.
     */
    dot->refCount += 1;
}

/*-
 *-----------------------------------------------------------------------
 * Dir_End --
 *	cleanup things for this module
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	none
 *-----------------------------------------------------------------------
 */
void
Dir_End()
{
#ifdef CLEANUP
    dot->refCount -= 1;
    Dir_Destroy(dot);
    Dir_ClearPath(&dirSearchPath);
    Lst_Destroy(&dirSearchPath, NOFREE);
    Dir_ClearPath(&openDirectories);
    Lst_Destroy(&openDirectories, NOFREE);
    Hash_DeleteTable(&mtimes);
#endif
}

/*-
 *-----------------------------------------------------------------------
 * DirFindName --
 *	See if the Path structure describes the same directory as the
 *	given one by comparing their names. Called from Dir_AddDir via
 *	Lst_Find when searching the list of open directories.
 *
 * Results:
 *	0 if it is the same. Non-zero otherwise
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static int
DirFindName (p, dname)
    void *p;	      /* Current name */
    void *dname;      /* Desired name */
{
    return strcmp(((Path *)p)->name, (char *)dname);
}

/*-
 *-----------------------------------------------------------------------
 * Dir_HasWildcards  --
 *	see if the given name has any wildcard characters in it
 *	be careful not to expand unmatching brackets or braces.
 *	XXX: This code is not 100% correct. ([^]] fails etc.)
 *	I really don't think that make(1) should be expanding
 *	patterns, because then you have to set a mechanism for
 *	escaping the expansion!
 *
 * Results:
 *	returns TRUE if the word should be expanded, FALSE otherwise
 *
 * Side Effects:
 *	none
 *-----------------------------------------------------------------------
 */
Boolean
Dir_HasWildcards (name)
    char          *name;	/* name to check */
{
    register char *cp;
    int wild = 0, brace = 0, bracket = 0;

    for (cp = name; *cp; cp++) {
	switch(*cp) {
	case '{':
	    brace++;
	    wild = 1;
	    break;
	case '}':
	    brace--;
	    break;
	case '[':
	    bracket++;
	    wild = 1;
	    break;
	case ']':
	    bracket--;
	    break;
	case '?':
	case '*':
	    wild = 1;
	    break;
	default:
	    break;
	}
    }
    return (wild && bracket == 0 && brace == 0);
}

/*-
 *-----------------------------------------------------------------------
 * DirMatchFiles --
 * 	Given a pattern and a Path structure, see if any files
 *	match the pattern and add their names to the 'expansions' list if
 *	any do. This is incomplete -- it doesn't take care of patterns like
 *	src / *src / *.c properly (just *.c on any of the directories), but it
 *	will do for now.
 *
 * Results:
 *	Always returns 0
 *
 * Side Effects:
 *	File names are added to the expansions lst. The directory will be
 *	fully hashed when this is done.
 *-----------------------------------------------------------------------
 */
static int
DirMatchFiles (pattern, p, expansions)
    char	  *pattern;   	/* Pattern to look for */
    Path	  *p;         	/* Directory to search */
    Lst	    	  expansions;	/* Place to store the results */
{
    Hash_Search	  search;   	/* Index into the directory's table */
    Hash_Entry	  *entry;   	/* Current entry in the table */
    Boolean 	  isDot;    	/* TRUE if the directory being searched is . */

    isDot = (*p->name == '.' && p->name[1] == '\0');

    for (entry = Hash_EnumFirst(&p->files, &search);
	 entry != (Hash_Entry *)NULL;
	 entry = Hash_EnumNext(&search))
    {
	/*
	 * See if the file matches the given pattern. Note we follow the UNIX
	 * convention that dot files will only be found if the pattern
	 * begins with a dot (note also that as a side effect of the hashing
	 * scheme, .* won't match . or .. since they aren't hashed).
	 */
	if (Str_Match(entry->name, pattern) &&
	    ((entry->name[0] != '.') ||
	     (pattern[0] == '.')))
	{
	    Lst_AtEnd(expansions,
			    (isDot ? estrdup(entry->name) :
			     str_concat(p->name, entry->name, '/')));
	}
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * DirExpandCurly --
 *	Expand curly braces like the C shell. Does this recursively.
 *	Note the special case: if after the piece of the curly brace is
 *	done there are no wildcard characters in the result, the result is
 *	placed on the list WITHOUT CHECKING FOR ITS EXISTENCE.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The given list is filled with the expansions...
 *
 *-----------------------------------------------------------------------
 */
static void
DirExpandCurly(word, brace, path, expansions)
    char    	  *word;    	/* Entire word to expand */
    char    	  *brace;   	/* First curly brace in it */
    Lst	    	  path;	    	/* Search path to use */
    Lst	    	  expansions;	/* Place to store the expansions */
{
    char    	  *end;	    	/* Character after the closing brace */
    char    	  *cp;	    	/* Current position in brace clause */
    char    	  *start;   	/* Start of current piece of brace clause */
    int	    	  bracelevel;	/* Number of braces we've seen. If we see a
				 * right brace when this is 0, we've hit the
				 * end of the clause. */
    char    	  *file;    	/* Current expansion */
    int	    	  otherLen; 	/* The length of the other pieces of the
				 * expansion (chars before and after the
				 * clause in 'word') */
    char    	  *cp2;	    	/* Pointer for checking for wildcards in
				 * expansion before calling Dir_Expand */

    start = brace+1;

    /*
     * Find the end of the brace clause first, being wary of nested brace
     * clauses.
     */
    for (end = start, bracelevel = 0; *end != '\0'; end++) {
	if (*end == '{') {
	    bracelevel++;
	} else if ((*end == '}') && (bracelevel-- == 0)) {
	    break;
	}
    }
    if (*end == '\0') {
	Error("Unterminated {} clause \"%s\"", start);
	return;
    } else {
	end++;
    }
    otherLen = brace - word + strlen(end);

    for (cp = start; cp < end; cp++) {
	/*
	 * Find the end of this piece of the clause.
	 */
	bracelevel = 0;
	while (*cp != ',') {
	    if (*cp == '{') {
		bracelevel++;
	    } else if ((*cp == '}') && (bracelevel-- <= 0)) {
		break;
	    }
	    cp++;
	}
	/*
	 * Allocate room for the combination and install the three pieces.
	 */
	file = emalloc(otherLen + cp - start + 1);
	if (brace != word) {
	    strncpy(file, word, brace-word);
	}
	if (cp != start) {
	    strncpy(&file[brace-word], start, cp-start);
	}
	strcpy(&file[(brace-word)+(cp-start)], end);

	/*
	 * See if the result has any wildcards in it. If we find one, call
	 * Dir_Expand right away, telling it to place the result on our list
	 * of expansions.
	 */
	for (cp2 = file; *cp2 != '\0'; cp2++) {
	    switch(*cp2) {
	    case '*':
	    case '?':
	    case '{':
	    case '[':
		Dir_Expand(file, path, expansions);
		goto next;
	    }
	}
	if (*cp2 == '\0') {
	    /*
	     * Hit the end w/o finding any wildcards, so stick the expansion
	     * on the end of the list.
	     */
	    Lst_AtEnd(expansions, file);
	} else {
	next:
	    free(file);
	}
	start = cp+1;
    }
}


/*-
 *-----------------------------------------------------------------------
 * DirExpandInt --
 *	Internal expand routine. Passes through the directories in the
 *	path one by one, calling DirMatchFiles for each. NOTE: This still
 *	doesn't handle patterns in directories...
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Things are added to the expansions list.
 *
 *-----------------------------------------------------------------------
 */
static void
DirExpandInt(word, path, expansions)
    char    	  *word;    	/* Word to expand */
    Lst	    	  path;	    	/* Path on which to look */
    Lst	    	  expansions;	/* Place to store the result */
{
    LstNode 	  ln;	    	/* Current node */
    Path	  *p;	    	/* Directory in the node */

    Lst_Open(path);
    while ((ln = Lst_Next(path)) != NULL) {
	p = (Path *)Lst_Datum(ln);
	DirMatchFiles(word, p, expansions);
    }
    Lst_Close(path);
}

/*-
 *-----------------------------------------------------------------------
 * DirPrintWord --
 *	Print a word in the list of expansions, followed by a space. 
 *	Callback for Dir_Expand when DEBUG(DIR), via Lst_ForEach.
 *-----------------------------------------------------------------------
 */
static void
DirPrintWord(word)
    void *word;
{
    printf("%s ", (char *)word);
}

/*-
 *-----------------------------------------------------------------------
 * Dir_Expand  --
 *	Expand the given word into a list of words by globbing it looking
 *	in the directories on the given search path.
 *
 * Results:
 *	A list of words consisting of the files which exist along the search
 *	path matching the given pattern.
 *
 * Side Effects:
 *	Directories may be opened. Who knows?
 *-----------------------------------------------------------------------
 */
void
Dir_Expand (word, path, expansions)
    char    *word;      /* the word to expand */
    Lst     path;   	/* the list of directories in which to find
			 * the resulting files */
    Lst	    expansions;	/* the list on which to place the results */
{
    char    	  *cp;

    if (DEBUG(DIR)) {
	printf("expanding \"%s\"...", word);
    }

    cp = strchr(word, '{');
    if (cp) {
	DirExpandCurly(word, cp, path, expansions);
    } else {
	cp = strchr(word, '/');
	if (cp) {
	    /*
	     * The thing has a directory component -- find the first wildcard
	     * in the string.
	     */
	    for (cp = word; *cp; cp++) {
		if (*cp == '?' || *cp == '[' || *cp == '*' || *cp == '{') {
		    break;
		}
	    }
	    if (*cp == '{') {
		/* This one will be fun.  */
		DirExpandCurly(word, cp, path, expansions);
		return;
	    } else if (*cp != '\0') {
		/* Back up to the start of the component.  */
		char  *dirpath;

		while (cp > word && *cp != '/')
		    cp--;
		if (cp != word) {
		    char sc;
		    /*
		     * If the glob isn't in the first component, try and find
		     * all the components up to the one with a wildcard.
		     */
		    sc = cp[1];
		    cp[1] = '\0';
		    dirpath = Dir_FindFile(word, path);
		    cp[1] = sc;
		    /*
		     * dirpath is null if can't find the leading component
		     * XXX: Dir_FindFile won't find internal components.
		     * i.e. if the path contains ../Etc/Object and we're
		     * looking for Etc, it won't be found. Ah well.
		     * Probably not important.
		     */
		    if (dirpath != NULL) {
		    	LIST temp;

			char *dp = &dirpath[strlen(dirpath) - 1];
			if (*dp == '/')
			    *dp = '\0';
			Lst_Init(&temp);
			Dir_AddDir(&temp, dirpath);
			DirExpandInt(cp+1, &temp, expansions);
			Lst_Destroy(&temp, NOFREE);
		    }
		} else
		    /* Start the search from the local directory.  */
		    DirExpandInt(word, path, expansions);
	    } else
		/* Return the file -- this should never happen.  */
		DirExpandInt(word, path, expansions);
	} else {
	    /* First the files in dot.  */
	    DirMatchFiles(word, dot, expansions);

	    /* Then the files in every other directory on the path.  */
	    DirExpandInt(word, path, expansions);
	}
    }
    if (DEBUG(DIR)) {
	Lst_Every(expansions, DirPrintWord);
	fputc('\n', stdout);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Dir_FindFile  --
 *	Find the file with the given name along the given search path.
 *
 * Results:
 *	The path to the file or NULL. This path is guaranteed to be in a
 *	different part of memory than name and so may be safely free'd.
 *
 * Side Effects:
 *	If the file is found in a directory which is not on the path
 *	already (either 'name' is absolute or it is a relative path
 *	[ dir1/.../dirn/file ] which exists below one of the directories
 *	already on the search path), its directory is added to the end
 *	of the path on the assumption that there will be more files in
 *	that directory later on. Sometimes this is true. Sometimes not.
 *-----------------------------------------------------------------------
 */
char *
Dir_FindFile (name, path)
    char    	  *name;    /* the file to find */
    Lst           path;	    /* the Lst of directories to search */
{
    register char *p1;	    /* pointer into p->name */
    register char *p2;	    /* pointer into name */
    LstNode       ln;	    /* a list element */
    register char *file;    /* the current filename to check */
    register Path *p;	    /* current path member */
    register char *cp;	    /* index of first slash, if any */
    Boolean	  hasSlash; /* true if 'name' contains a / */
    struct stat	  stb;	    /* Buffer for stat, if necessary */
    Hash_Entry	  *entry;   /* Entry for mtimes table */

    /*
     * Find the final component of the name and note whether it has a
     * slash in it (the name, I mean)
     */
    cp = strrchr (name, '/');
    if (cp) {
	hasSlash = TRUE;
	cp += 1;
    } else {
	hasSlash = FALSE;
	cp = name;
    }

    if (DEBUG(DIR)) {
	printf("Searching for %s...", name);
    }
    /*
     * No matter what, we always look for the file in the current directory
     * before anywhere else and we *do not* add the ./ to it if it exists.
     * This is so there are no conflicts between what the user specifies
     * (fish.c) and what pmake finds (./fish.c).
     */
    if ((!hasSlash || (cp - name == 2 && *name == '.')) &&
	(Hash_FindEntry (&dot->files, cp) != (Hash_Entry *)NULL)) {
	    if (DEBUG(DIR)) {
		printf("in '.'\n");
	    }
	    hits += 1;
	    dot->hits += 1;
	    return (estrdup (name));
    }

    Lst_Open(path);

    /*
     * We look through all the directories on the path seeking one which
     * contains the final component of the given name and whose final
     * component(s) match the name's initial component(s). If such a beast
     * is found, we concatenate the directory name and the final component
     * and return the resulting string. If we don't find any such thing,
     * we go on to phase two...
     */
    while ((ln = Lst_Next (path)) != NULL) {
	p = (Path *)Lst_Datum(ln);
	if (DEBUG(DIR)) {
	    printf("%s...", p->name);
	}
	if (Hash_FindEntry (&p->files, cp) != (Hash_Entry *)NULL) {
	    if (DEBUG(DIR)) {
		printf("here...");
	    }
	    if (hasSlash) {
		/*
		 * If the name had a slash, its initial components and p's
		 * final components must match. This is false if a mismatch
		 * is encountered before all of the initial components
		 * have been checked (p2 > name at the end of the loop), or
		 * we matched only part of one of the components of p
		 * along with all the rest of them (*p1 != '/').
		 */
		p1 = p->name + strlen (p->name) - 1;
		p2 = cp - 2;
		while (p2 >= name && p1 >= p->name && *p1 == *p2) {
		    p1 -= 1; p2 -= 1;
		}
		if (p2 >= name || (p1 >= p->name && *p1 != '/')) {
		    if (DEBUG(DIR)) {
			printf("component mismatch -- continuing...");
		    }
		    continue;
		}
	    }
	    file = str_concat(p->name, cp, '/');
	    if (DEBUG(DIR)) {
		printf("returning %s\n", file);
	    }
	    Lst_Close (path);
	    p->hits += 1;
	    hits += 1;
	    return (file);
	} else if (hasSlash) {
	    /*
	     * If the file has a leading path component and that component
	     * exactly matches the entire name of the current search
	     * directory, we assume the file doesn't exist and return NULL.
	     */
	    for (p1 = p->name, p2 = name; *p1 && *p1 == *p2; p1++, p2++) {
		continue;
	    }
	    if (*p1 == '\0' && p2 == cp - 1) {
		if (DEBUG(DIR)) {
		    printf("must be here but isn't -- returing NULL\n");
		}
		Lst_Close (path);
		return ((char *) NULL);
	    }
	}
    }

    /*
     * We didn't find the file on any existing members of the directory.
     * If the name doesn't contain a slash, that means it doesn't exist.
     * If it *does* contain a slash, however, there is still hope: it
     * could be in a subdirectory of one of the members of the search
     * path. (eg. /usr/include and sys/types.h. The above search would
     * fail to turn up types.h in /usr/include, but it *is* in
     * /usr/include/sys/types.h) If we find such a beast, we assume there
     * will be more (what else can we assume?) and add all but the last
     * component of the resulting name onto the search path (at the
     * end). This phase is only performed if the file is *not* absolute.
     */
    if (!hasSlash) {
	if (DEBUG(DIR)) {
	    printf("failed.\n");
	}
	misses += 1;
	return ((char *) NULL);
    }

    if (*name != '/') {
	Boolean	checkedDot = FALSE;

	if (DEBUG(DIR)) {
	    printf("failed. Trying subdirectories...");
	}
	Lst_Open(path);
	while ((ln = Lst_Next (path)) != NULL) {
	    p = (Path *)Lst_Datum(ln);
	    if (p != dot) {
		file = str_concat(p->name, name, '/');
	    } else {
		/*
		 * Checking in dot -- DON'T put a leading ./ on the thing.
		 */
		file = estrdup(name);
		checkedDot = TRUE;
	    }
	    if (DEBUG(DIR)) {
		printf("checking %s...", file);
	    }


	    if (stat (file, &stb) == 0) {
		if (DEBUG(DIR)) {
		    printf("got it.\n");
		}

		Lst_Close (path);

		/*
		 * We've found another directory to search. We know there's
		 * a slash in 'file' because we put one there. We nuke it after
		 * finding it and call Dir_AddDir to add this new directory
		 * onto the existing search path. Once that's done, we restore
		 * the slash and triumphantly return the file name, knowing
		 * that should a file in this directory every be referenced
		 * again in such a manner, we will find it without having to do
		 * numerous numbers of access calls. Hurrah!
		 */
		cp = strrchr (file, '/');
		*cp = '\0';
		Dir_AddDir (path, file);
		*cp = '/';

		/*
		 * Save the modification time so if it's needed, we don't have
		 * to fetch it again.
		 */
		if (DEBUG(DIR)) {
		    printf("Caching %s for %s\n", Targ_FmtTime(stb.st_mtime),
			    file);
		}
		entry = Hash_CreateEntry(&mtimes, (char *) file,
					 (Boolean *)NULL);
		/* XXX */
		Hash_SetValue(entry, (void *)((long)stb.st_mtime));
		nearmisses += 1;
		return (file);
	    } else {
		free (file);
	    }
	}

	if (DEBUG(DIR)) {
	    printf("failed. ");
	}
	Lst_Close (path);

	if (checkedDot) {
	    /*
	     * Already checked by the given name, since . was in the path,
	     * so no point in proceeding...
	     */
	    if (DEBUG(DIR)) {
		printf("Checked . already, returning NULL\n");
	    }
	    return(NULL);
	}
    }

    /*
     * Didn't find it that way, either. Sigh. Phase 3. Add its directory
     * onto the search path in any case, just in case, then look for the
     * thing in the hash table. If we find it, grand. We return a new
     * copy of the name. Otherwise we sadly return a NULL pointer. Sigh.
     * Note that if the directory holding the file doesn't exist, this will
     * do an extra search of the final directory on the path. Unless something
     * weird happens, this search won't succeed and life will be groovy.
     *
     * Sigh. We cannot add the directory onto the search path because
     * of this amusing case:
     * $(INSTALLDIR)/$(FILE): $(FILE)
     *
     * $(FILE) exists in $(INSTALLDIR) but not in the current one.
     * When searching for $(FILE), we will find it in $(INSTALLDIR)
     * b/c we added it here. This is not good...
     */
#ifdef notdef
    cp[-1] = '\0';
    Dir_AddDir (path, name);
    cp[-1] = '/';

    bigmisses += 1;
    ln = Lst_Last(path);
    if (ln == NULL)
	return NULL;
    else
	p = (Path *)Lst_Datum(ln);

    if (Hash_FindEntry (&p->files, cp) != (Hash_Entry *)NULL) {
	return (estrdup (name));
    } else {
	return ((char *) NULL);
    }
#else /* !notdef */
    if (DEBUG(DIR)) {
	printf("Looking for \"%s\"...", name);
    }

    bigmisses += 1;
    entry = Hash_FindEntry(&mtimes, name);
    if (entry != (Hash_Entry *)NULL) {
	if (DEBUG(DIR)) {
	    printf("got it (in mtime cache)\n");
	}
	return(estrdup(name));
    } else if (stat (name, &stb) == 0) {
	entry = Hash_CreateEntry(&mtimes, name, (Boolean *)NULL);
	if (DEBUG(DIR)) {
	    printf("Caching %s for %s\n", Targ_FmtTime(stb.st_mtime),
		    name);
	}
	/* XXX */
	Hash_SetValue(entry, (void *)(long)stb.st_mtime);
	return (estrdup (name));
    } else {
	if (DEBUG(DIR)) {
	    printf("failed. Returning NULL\n");
	}
	return ((char *)NULL);
    }
#endif /* notdef */
}

/*-
 *-----------------------------------------------------------------------
 * Dir_MTime  --
 *	Find the modification time of the file described by gn along the
 *	search path dirSearchPath.
 *
 * Results:
 *	TRUE if file exists.
 *
 * Side Effects:
 *	The modification time is placed in the node's mtime slot.
 *	If the node didn't have a path entry before, and Dir_FindFile
 *	found one for it, the full name is placed in the path slot.
 *-----------------------------------------------------------------------
 */
Boolean
Dir_MTime(gn)
    GNode         *gn;	      /* the file whose modification time is
			       * desired */
{
    char          *fullName;  /* the full pathname of name */
    struct stat	  stb;	      /* buffer for finding the mod time */
    Hash_Entry	  *entry;
    Boolean 	  exists;

    if (gn->type & OP_ARCHV)
	return Arch_MTime(gn);
    else if (gn->path == NULL)
	fullName = Dir_FindFile(gn->name, &dirSearchPath);
    else
	fullName = gn->path;

    if (fullName == (char *)NULL) {
	fullName = estrdup(gn->name);
    }

    entry = Hash_FindEntry(&mtimes, fullName);
    if (entry != (Hash_Entry *)NULL) {
	/*
	 * Only do this once -- the second time folks are checking to
	 * see if the file was actually updated, so we need to actually go
	 * to the file system.
	 */
	if (DEBUG(DIR)) {
	    printf("Using cached time %s for %s\n",
		    Targ_FmtTime((time_t)(long)Hash_GetValue(entry)), fullName);
	}
	stb.st_mtime = (time_t)(long)Hash_GetValue(entry);
	Hash_DeleteEntry(&mtimes, entry);
	exists = TRUE;
    } else if (stat (fullName, &stb) == 0) {
    	/* XXX forces make to differentiate between the epoch and
	 * non-existent files by kludging the timestamp slightly. */
    	if (stb.st_mtime == OUT_OF_DATE)
		stb.st_mtime++;
	exists = TRUE;
    } else {
	if (gn->type & OP_MEMBER) {
	    if (fullName != gn->path)
		free(fullName);
	    return Arch_MemMTime (gn);
	} else {
	    stb.st_mtime = OUT_OF_DATE;
	    exists = FALSE;
	}
    }
    if (fullName && gn->path == (char *)NULL) {
	gn->path = fullName;
    }

    gn->mtime = stb.st_mtime;
    return exists;
}

/*-
 *-----------------------------------------------------------------------
 * Dir_AddDir --
 *	Add the given name to the end of the given path. The order of
 *	the arguments is backwards so ParseDoDependency can do a
 *	Lst_ForEach of its list of paths...
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	A structure is added to the list and the directory is
 *	read and hashed.
 *-----------------------------------------------------------------------
 */
void
Dir_AddDir (path, name)
    Lst           path;	      /* the path to which the directory should be
			       * added */
    char          *name;      /* the name of the directory to add */
{
    LstNode       ln;	      /* node in case Path structure is found */
    register Path *p;	      /* pointer to new Path structure */
    DIR     	  *d;	      /* for reading directory */
    register struct dirent *dp; /* entry in directory */

    ln = Lst_Find(&openDirectories, DirFindName, name);
    if (ln != NULL) {
	p = (Path *)Lst_Datum(ln);
	if (Lst_Member(path, p) == NULL) {
	    p->refCount += 1;
	    Lst_AtEnd(path, p);
	}
    } else {
	if (DEBUG(DIR)) {
	    printf("Caching %s...", name);
	    fflush(stdout);
	}

	if ((d = opendir (name)) != (DIR *) NULL) {
	    p = (Path *) emalloc (sizeof (Path));
	    p->name = estrdup (name);
	    p->hits = 0;
	    p->refCount = 1;
	    Hash_InitTable (&p->files, -1);

	    /*
	     * Skip the first two entries -- these will *always* be . and ..
	     */
	    (void)readdir(d);
	    (void)readdir(d);

	    while ((dp = readdir (d)) != (struct dirent *) NULL) {
#if defined(sun) && defined(d_ino) /* d_ino is a sunos4 #define for d_fileno */
		/*
		 * The sun directory library doesn't check for a 0 inode
		 * (0-inode slots just take up space), so we have to do
		 * it ourselves.
		 */
		if (dp->d_fileno == 0) {
		    continue;
		}
#endif /* sun && d_ino */
		(void)Hash_CreateEntry(&p->files, dp->d_name, (Boolean *)NULL);
	    }
	    (void) closedir (d);
	    Lst_AtEnd(&openDirectories, p);
	    Lst_AtEnd(path, p);
	}
	if (DEBUG(DIR)) {
	    printf("done\n");
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Dir_CopyDir --
 *	Callback function for duplicating a search path via Lst_Duplicate.
 *	Ups the reference count for the directory.
 *
 * Results:
 *	Returns the Path it was given.
 *
 * Side Effects:
 *	The refCount of the path is incremented.
 *
 *-----------------------------------------------------------------------
 */
void *
Dir_CopyDir(p)
    void *p;
{
    ((Path *)p)->refCount += 1;

    return p;
}

/*-
 *-----------------------------------------------------------------------
 * Dir_MakeFlags --
 *	Make a string by taking all the directories in the given search
 *	path and preceding them by the given flag. Used by the suffix
 *	module to create variables for compilers based on suffix search
 *	paths.
 *
 * Results:
 *	The string mentioned above. Note that there is no space between
 *	the given flag and each directory. The empty string is returned if
 *	Things don't go well.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
char *
Dir_MakeFlags(flag, path)
    char	  *flag;  /* flag which should precede each directory */
    Lst	    	  path;	  /* list of directories */
{
    LstNode	  ln;	  /* the node of the current directory */
    BUFFER	  buf;

    Buf_Init(&buf, 0);

    for (ln = Lst_First(path); ln != NULL; ln = Lst_Adv(ln)) {
	    Buf_AddString(&buf, flag);
	    Buf_AddString(&buf, ((Path *)Lst_Datum(ln))->name);
	    Buf_AddSpace(&buf);
    }

    return Buf_Retrieve(&buf);
}

/*-
 *-----------------------------------------------------------------------
 * Dir_Destroy --
 *	Nuke a directory descriptor, if possible. Callback procedure
 *	for the suffixes module when destroying a search path.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If no other path references this directory (refCount == 0),
 *	the Path and all its data are freed.
 *
 *-----------------------------------------------------------------------
 */
void
Dir_Destroy (pp)
    void *pp;	    /* The directory descriptor to nuke */
{
    Path    	  *p = (Path *) pp;
    p->refCount -= 1;

    if (p->refCount == 0) {
	LstNode	ln;

	ln = Lst_Member(&openDirectories, p);
	Lst_Remove(&openDirectories, ln);

	Hash_DeleteTable (&p->files);
	free(p->name);
	free(p);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Dir_ClearPath --
 *	Clear out all elements of the given search path. This is different
 *	from destroying the list, notice.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The path is set to the empty list.
 *
 *-----------------------------------------------------------------------
 */
void
Dir_ClearPath(path)
    Lst	    path; 	/* Path to clear */
{
    Path    *p;
    while ((p = (Path *)Lst_DeQueue(path)) != NULL)
	Dir_Destroy(p);
}


/*-
 *-----------------------------------------------------------------------
 * Dir_Concat --
 *	Concatenate two paths, adding the second to the end of the first.
 *	Makes sure to avoid duplicates.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Reference counts for added dirs are upped.
 *
 *-----------------------------------------------------------------------
 */
void
Dir_Concat(path1, path2)
    Lst	    path1;  	/* Dest */
    Lst	    path2;  	/* Source */
{
    LstNode ln;
    Path    *p;

    for (ln = Lst_First(path2); ln != NULL; ln = Lst_Adv(ln)) {
	p = (Path *)Lst_Datum(ln);
	if (Lst_Member(path1, p) == NULL) {
	    p->refCount += 1;
	    Lst_AtEnd(path1, p);
	}
    }
}

/********** DEBUG INFO **********/
void
Dir_PrintDirectories()
{
    LstNode	ln;
    Path	*p;

    printf ("#*** Directory Cache:\n");
    printf ("# Stats: %d hits %d misses %d near misses %d losers (%d%%)\n",
	      hits, misses, nearmisses, bigmisses,
	      (hits+bigmisses+nearmisses ?
	       hits * 100 / (hits + bigmisses + nearmisses) : 0));
    printf ("# %-20s referenced\thits\n", "directory");
    Lst_Open(&openDirectories);
    while ((ln = Lst_Next(&openDirectories)) != NULL) {
	p = (Path *)Lst_Datum(ln);
	printf("# %-20s %10d\t%4d\n", p->name, p->refCount, p->hits);
    }
    Lst_Close(&openDirectories);
}

static void 
DirPrintDir(p)
    void *p;
{
    printf("%s ", ((Path *)p)->name);
}

void
Dir_PrintPath(path)
    Lst	path;
{
    Lst_Every(path, DirPrintDir);
}
