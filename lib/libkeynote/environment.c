/*  $OpenBSD: environment.c,v 1.7 1999/10/09 06:59:37 angelos Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
 * in April-May 1998
 *
 * Copyright (C) 1998, 1999 by Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#if STDC_HEADERS
#include <string.h>
#if !defined(HAVE_MEMCPY)
#define memcpy(d, s, n) bcopy ((s), (d), (n))
#endif /* !HAVE_MEMCPY */
#endif /* STDC_HEADERS */

#if HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#if HAVE_IO_H
#include <io.h>
#elif HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_IO_H */

#include "header.h"
#include "keynote.h"
#include "assertion.h"

static int sessioncounter = 0;

char **keynote_values = (char **) NULL;
char *keynote_privkey = (char *) NULL;

struct assertion *keynote_current_assertion = (struct assertion *) NULL;

struct environment *keynote_init_list = (struct environment *) NULL;
struct environment *keynote_temp_list = (struct environment *) NULL;

struct keylist *keynote_keypred_keylist = (struct keylist *) NULL;

struct keynote_session *keynote_sessions[SESSIONTABLESIZE];
struct keynote_session *keynote_current_session = NULL;

int keynote_exceptionflag = 0;
int keynote_used_variable = 0;
int keynote_returnvalue = 0;
int keynote_justrecord = 0;
int keynote_donteval = 0;
int keynote_errno = 0;

/*
 * Construct the _ACTION_AUTHORIZERS variable value.
 */
static char *
keynote_get_action_authorizers(char *name)
{
    struct keylist *kl;
    int len;

    if (!strcmp(name, KEYNOTE_CALLBACK_CLEANUP) ||
        !strcmp(name, KEYNOTE_CALLBACK_INITIALIZE))
    {
        if (keynote_current_session->ks_authorizers_cache != (char *) NULL)
	{
	    free(keynote_current_session->ks_authorizers_cache);
	    keynote_current_session->ks_authorizers_cache = (char *) NULL;
	}

	return "";
    }

    if (keynote_current_session->ks_authorizers_cache != (char *) NULL)
      return keynote_current_session->ks_authorizers_cache;

    for (len = 0, kl = keynote_current_session->ks_action_authorizers;
	 kl != (struct keylist *) NULL;
	 kl = kl->key_next)
      if (kl->key_stringkey != (char *) NULL)
        len += strlen(kl->key_stringkey) + 1;

    if (len == 0)
      return "";

    keynote_current_session->ks_authorizers_cache = (char *) calloc(len, sizeof(char));
    if (keynote_current_session->ks_authorizers_cache == (char *) NULL)
    {
	keynote_errno = ERROR_MEMORY;
	return (char *) NULL;
    }

    for (len = 0, kl = keynote_current_session->ks_action_authorizers;
	 kl != (struct keylist *) NULL;
	 kl = kl->key_next)
      if (kl->key_stringkey != (char *) NULL)
      {
	  sprintf(keynote_current_session->ks_authorizers_cache + len, "%s,",
		  kl->key_stringkey);
	  len += strlen(kl->key_stringkey) + 1;
      }

    keynote_current_session->ks_authorizers_cache[len - 1] = '\0';
    return keynote_current_session->ks_authorizers_cache;
}

/*
 * Construct the _VALUES variable value.
 */
static char *
keynote_get_values(char *name)
{
    int i, len;

    if (!strcmp(name, KEYNOTE_CALLBACK_CLEANUP) ||
        !strcmp(name, KEYNOTE_CALLBACK_INITIALIZE))
    {
        if (keynote_current_session->ks_values_cache != (char *) NULL)
	{
	    free(keynote_current_session->ks_values_cache);
	    keynote_current_session->ks_values_cache = (char *) NULL;
	}

	return "";
    }

    if (keynote_current_session->ks_values_cache != (char *) NULL)
      return keynote_current_session->ks_values_cache;

    for (len = 0, i = 0; i < keynote_current_session->ks_values_num; i++)
      len += strlen(keynote_current_session->ks_values[i]) + 1;

    keynote_current_session->ks_values_cache = (char *) calloc(len,
							       sizeof(char));
    if (keynote_current_session->ks_values_cache == (char *) NULL)
    {
	keynote_errno = ERROR_MEMORY;
	return (char *) NULL;
    }

    if (len == 0)
      return "";

    for (len = 0, i = 0; i < keynote_current_session->ks_values_num; i++)
    {
	sprintf(keynote_current_session->ks_values_cache + len, "%s,",
		keynote_current_session->ks_values[i]);
	len += strlen(keynote_current_session->ks_values[i]) + 1;
    }

    keynote_current_session->ks_values_cache[len - 1] = '\0';
    return keynote_current_session->ks_values_cache;
}

/*
 * Free an environment structure.
 */
void
keynote_free_env(struct environment *en)
{
    if (en == (struct environment *) NULL)
      return;

    if (en->env_name != (char *) NULL)
      free(en->env_name);

    if (en->env_flags & ENVIRONMENT_FLAG_REGEX)
      regfree(&(en->env_regex));

    if (!(en->env_flags & ENVIRONMENT_FLAG_FUNC))
    {
        if (en->env_value != (char *) NULL)
	  free(en->env_value);
    }
    else
      ((char * (*) (char *))en->env_value)(KEYNOTE_CALLBACK_CLEANUP);

    free(en);
}

/*
 * Lookup for variable "name" in the hash table. If hashsize is 1,
 * then the second argument is actually a pointer to a list. Last
 * argument specifies case-insensitivity.
 */
char *
keynote_env_lookup(char *name, struct environment **table, u_int hashsize)
{
    struct environment *en;

    for (en = table[keynote_stringhash(name, hashsize)]; 
	 en != (struct environment *) NULL;
	 en = en->env_next)
      if (((en->env_flags & ENVIRONMENT_FLAG_REGEX) &&
	   (regexec(&(en->env_regex), name, 0, (regmatch_t *) NULL, 0) ==
	    0)) || (!strcmp(name, en->env_name)))
      {
	  if ((en->env_flags & ENVIRONMENT_FLAG_FUNC) &&
	      (en->env_value != (char *) NULL))
	    return ((char * (*) (char *)) en->env_value)(name);
	  else
	    return en->env_value;
      }

    return (char *) NULL;
}

/*
 * Delete a variable from hash table. Return RESULT_TRUE if the deletion was
 * successful, and RESULT_FALSE if the variable was not found.
 */
int
keynote_env_delete(char *name, struct environment **table, u_int hashsize)
{
    struct environment *en, *en2;
    u_int h;
    
    h = keynote_stringhash(name, hashsize);
    
    if (table[h] != (struct environment *) NULL)
    {
	if (!strcmp(table[h]->env_name, name))
	{
	    en = table[h];
	    table[h] = en->env_next;
	    keynote_free_env(en);
	    return RESULT_TRUE;
	}
	else
	  for (en = table[h]; 
	       en->env_next != (struct environment *) NULL;
	       en = en->env_next)
	    if (!strcmp(en->env_next->env_name, name))
	    {
		en2 = en->env_next;
		en->env_next = en2->env_next;
		keynote_free_env(en2);
		return RESULT_TRUE;
	    }
    }

   return RESULT_FALSE;
}

/* 
 * Add a new variable in hash table. Return RESULT_TRUE on success,
 * ERROR_MEMORY on failure. If hashsize is 1, second argument is
 * actually a pointer to a list. The arguments are duplicated.
 */
int
keynote_env_add(char *name, char *value, struct environment **table,
		u_int hashsize, int flags)
{
    struct environment *en;
    u_int h, i;
    
    en = calloc(1, sizeof(struct environment));
    if (en == (struct environment *) NULL)
    {
	keynote_errno = ERROR_MEMORY;
	return -1;
    }

    en->env_name = strdup(name);
    if (en->env_name == (char *) NULL)
    {
	keynote_free_env(en);
	keynote_errno = ERROR_MEMORY;
	return -1;
    }

    if (flags & ENVIRONMENT_FLAG_REGEX) /* Regular expression for name */
    {
	if ((i = regcomp(&(en->env_regex), name, REG_EXTENDED)) != 0)
	{
	    keynote_free_env(en);
	    if (i == REG_ESPACE)
	      keynote_errno = ERROR_MEMORY;
	    else
	      keynote_errno = ERROR_SYNTAX;
	    return -1;
	}
        en->env_flags |= ENVIRONMENT_FLAG_REGEX;
    }

    if (flags & ENVIRONMENT_FLAG_FUNC) /* Callback registration */
    {
	en->env_value = value;
	en->env_flags |= ENVIRONMENT_FLAG_FUNC;
        ((char * (*) (char *))en->env_value)(KEYNOTE_CALLBACK_INITIALIZE);
	if (keynote_errno != 0)
	{
	    keynote_free_env(en);
	    return -1;
	}
    }
    else
    {
	en->env_value = strdup(value);
	if (en->env_value == (char *) NULL)
	{
	    keynote_free_env(en);
	    keynote_errno = ERROR_MEMORY;
	    return -1;
	}
    }

    /* 
     * This means that new assignments of existing variable will override 
     * the old ones.
     */
    h = keynote_stringhash(name, hashsize);
    en->env_next = table[h];
    table[h] = en;
    return RESULT_TRUE;
}

/*
 * Cleanup an environment table.
 */
void
keynote_env_cleanup(struct environment **table, u_int hashsize)
{
    struct environment *en2;

    if ((hashsize == 0) || (table == (struct environment **) NULL))
      return;
    
    while (hashsize > 0)
    {
	while (table[hashsize - 1] != (struct environment *) NULL)
	{
	    en2 = table[hashsize - 1]->env_next;
	    keynote_free_env(table[hashsize - 1]);
	    table[hashsize - 1] = en2;
	}

	hashsize--;
    }
}

/*
 * Zero out the attribute structures, seed the RNG.
 */
static int
keynote_init_environment(void)
{
#ifdef CRYPTO
    int cnt = KEYNOTE_RAND_INIT_LEN, i;

    do
    {
        if ((i = RAND_load_file(KEYNOTERNDFILENAME, cnt)) <= 0)
        {
            keynote_errno = ERROR_MEMORY;
	    return -1;
        }
    
        cnt -= i;   
    } while (cnt > 0);
#endif /* CRYPTO */

    memset(keynote_current_session->ks_env_table, 0,
	   HASHTABLESIZE * sizeof(struct environment *));
    memset(keynote_current_session->ks_assertion_table, 0,
	   HASHTABLESIZE * sizeof(struct assertion *));
    keynote_current_session->ks_env_regex = (struct environment *) NULL;

    if (keynote_env_add("_ACTION_AUTHORIZERS",
			(char *) keynote_get_action_authorizers,
			keynote_current_session->ks_env_table, HASHTABLESIZE,
			ENVIRONMENT_FLAG_FUNC) != RESULT_TRUE)
      return -1;

    if (keynote_env_add("_VALUES", (char *) keynote_get_values,
			keynote_current_session->ks_env_table, HASHTABLESIZE,
			ENVIRONMENT_FLAG_FUNC) != RESULT_TRUE)
      return -1;

    return RESULT_TRUE;
}

/*
 * Return the index of argument in keynote_values[].
 */
int
keynote_retindex(char *s)
{
    int i;
    
    for (i = 0; i < keynote_current_session->ks_values_num; i++)
      if (!strcmp(s, keynote_current_session->ks_values[i]))
	return i;

    return -1;
}

/*
 * Find a session by its id.
 */
struct keynote_session *
keynote_find_session(int sessid)
{
    unsigned int h = sessid % SESSIONTABLESIZE;
    struct keynote_session *ks;
    
    for (ks = keynote_sessions[h];
	 ks != (struct keynote_session *) NULL;
	 ks = ks->ks_next)
      if (ks->ks_id == sessid)
	return ks;

    return (struct keynote_session *) NULL;
}

/*
 * Add a session in the hash table.
 */
static void
keynote_add_session(struct keynote_session *ks)
{
    unsigned int h = ks->ks_id % SESSIONTABLESIZE;

    ks->ks_next = keynote_sessions[h];
    if (ks->ks_next != (struct keynote_session *) NULL)
      ks->ks_next->ks_prev = ks;

    keynote_sessions[h] = ks;
}

/*
 * Initialize a KeyNote session.
 */
int
kn_init(void)
{
    keynote_errno = 0;
    keynote_current_session = (struct keynote_session *) calloc(1, sizeof(struct keynote_session));
    if (keynote_current_session == (struct keynote_session *) NULL)
    {
	keynote_errno = ERROR_MEMORY;
	return -1;
    }

    while (keynote_find_session(sessioncounter) !=
	   (struct keynote_session *) NULL)
    {
	sessioncounter++;
	if (sessioncounter < 0)
	  sessioncounter = 0;
    }

    keynote_current_session->ks_id = sessioncounter++;
    keynote_init_environment();
    keynote_add_session(keynote_current_session);
    return keynote_current_session->ks_id;
}

/*
 * Cleanup the action environment.
 */
int
kn_cleanup_action_environment(int sessid)
{
    struct keynote_session *ks;

    keynote_errno = 0;
    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return -1;
	}
    }

    ks = keynote_current_session;

    /* Cleanup environment */
    keynote_env_cleanup(ks->ks_env_table, HASHTABLESIZE);
    keynote_env_cleanup(&(ks->ks_env_regex), 1);

    return 0;
}

/*
 * Close a session.
 */
int
kn_close(int sessid)
{
    struct keynote_session *ks;
    struct assertion *as, *as2;
    int i;

    keynote_errno = 0;
    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return -1;
	}
    }

    ks = keynote_current_session;

    /* Cleanup environment -- no point using kn_cleanup_action_environment() */
    keynote_env_cleanup(ks->ks_env_table, HASHTABLESIZE);
    keynote_env_cleanup(&(ks->ks_env_regex), 1);

    /* Cleanup assertions */
    for (i = 0; i < HASHTABLESIZE; i++)
      for (as = ks->ks_assertion_table[i];
	   as != (struct assertion *) NULL;
	   as = as2)
      {
	  as2 = as->as_next;
	  keynote_free_assertion(as);
      }

    /* Cleanup action authorizers */
    keynote_keylist_free(ks->ks_action_authorizers);

    /* Unlink from chain */
    if (ks->ks_prev == (struct keynote_session *) NULL)
    {
	keynote_sessions[ks->ks_id % SESSIONTABLESIZE] = ks->ks_next;
	if (ks->ks_next != (struct keynote_session *) NULL)
	  ks->ks_next->ks_prev = (struct keynote_session *) NULL;
	
    }
    else
    {
	ks->ks_prev->ks_next = ks->ks_next;
	if (ks->ks_next != (struct keynote_session *) NULL)
	  ks->ks_next->ks_prev = ks->ks_prev;
    }
    
    free(ks);
    keynote_current_session = (struct keynote_session *) NULL;
    return 0;
}
	
/*
 * Add an action attribute.
 */
int
kn_add_action(int sessid, char *name, char *value, int flags)
{
    int i;

    keynote_errno = 0;
    if ((name == (char *) NULL) || (value == (char *) NULL) ||
	(name[0] == '_'))
    {
	keynote_errno = ERROR_SYNTAX;
	return -1;
    }

    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return -1;
	}
    }

    if (flags & ENVIRONMENT_FLAG_REGEX)
      i = keynote_env_add(name, value, 
			  &(keynote_current_session->ks_env_regex), 1, flags);
    else
      i = keynote_env_add(name, value, keynote_current_session->ks_env_table,
			  HASHTABLESIZE, flags);

    if (i == RESULT_TRUE)
      return 0;
    else
      return -1;
}

/*
 * Remove an action attribute.
 */
int
kn_remove_action(int sessid, char *name)
{
    int i;

    keynote_errno = 0;
    if ((name == (char *) NULL) || (name[0] == '_'))
    {
	keynote_errno = ERROR_SYNTAX;
	return -1;
    }

    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return -1;
	}
    }

    i = keynote_env_delete(name, keynote_current_session->ks_env_table,
			   HASHTABLESIZE);
    if (i == RESULT_TRUE)
      return 0;

    i = keynote_env_delete(name, &(keynote_current_session->ks_env_regex),
			   HASHTABLESIZE);
    if (i == RESULT_TRUE)
      return 0;

    keynote_errno = ERROR_NOTFOUND;
    return -1;
}

/*
 * Execute a query.
 */
int
kn_do_query(int sessid, char **returnvalues, int numvalues)
{
    struct assertion *as;
    int i;

    keynote_errno = 0;
    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return -1;
	}
    }

    /* Check that we have at least one action authorizer */
    if (keynote_current_session->ks_action_authorizers ==
	(struct keylist *) NULL)
    {
	keynote_errno = ERROR_NOTFOUND;
	return -1;
    }

    /* 
     * We may use already set returnvalues, or use new ones,
     * but we must have some before we can evaluate.
     */
    if ((returnvalues == (char **) NULL) &&
	(keynote_current_session->ks_values == (char **) NULL))
    {
	keynote_errno = ERROR_SYNTAX;
	return -1;
    }

    /* Replace any existing returnvalues */
    if (returnvalues != (char **) NULL)
    {
	keynote_current_session->ks_values = returnvalues;
	keynote_current_session->ks_values_num = numvalues;
    }

    /* Reset assertion state from any previous queries */
    for (i = 0; i < HASHTABLESIZE; i++)
      for (as = keynote_current_session->ks_assertion_table[i];
	   as != (struct assertion *) NULL;
	   as = as->as_next)
      {
	  as->as_kresult = KRESULT_UNTOUCHED;
	  as->as_result = 0;
	  as->as_internalflags &= ~ASSERT_IFLAG_PROCESSED;
	  as->as_error = 0;
	  if (as->as_internalflags & ASSERT_IFLAG_WEIRDSIG)
	    as->as_sigresult = SIGRESULT_UNTOUCHED;
      }

    return keynote_evaluate_query();
}

/*
 * Return assertions that failed, by error type.
 */
int
kn_get_failed(int sessid, int type, int num)
{
    struct assertion *as;
    int i;

    keynote_errno = 0;
    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return -1;
	}
    }

    for (i = 0; i < HASHTABLESIZE; i++)
      for (as = keynote_current_session->ks_assertion_table[i];
	   as != (struct assertion *) NULL;
	   as = as->as_next)
	switch (type)
	{
	    case KEYNOTE_ERROR_ANY:
		if ((as->as_error != 0) ||
		    ((as->as_sigresult != SIGRESULT_TRUE) &&
		     !(as->as_sigresult == SIGRESULT_UNTOUCHED) &&
		     !(as->as_flags & ASSERT_FLAG_LOCAL)))
		  if (num-- == 0)  /* Return it if it's the num-th found */
		    return as->as_id;
		break;

	    case KEYNOTE_ERROR_MEMORY:
		if (as->as_error == ERROR_MEMORY)
		  if (num-- == 0)
		    return as->as_id;
		break;

	    case KEYNOTE_ERROR_SYNTAX:
		if (as->as_error == ERROR_SYNTAX)
		  if (num-- == 0)
		    return as->as_id;
		break;

	    case KEYNOTE_ERROR_SIGNATURE:
		if ((as->as_sigresult != SIGRESULT_TRUE) &&
		    !(as->as_sigresult == SIGRESULT_UNTOUCHED) &&
		    !(as->as_flags & ASSERT_FLAG_LOCAL))
		  if (num-- == 0)
		    return as->as_id;
		break;
	}

    keynote_errno = ERROR_NOTFOUND;
    return -1;
}

/*
 * Simple API for doing a single KeyNote query.
 */
int
kn_query(struct environment *env, char **retvalues, int numval,
	 char **trusted, int *trustedlen, int numtrusted,
	 char **untrusted, int *untrustedlen, int numuntrusted,
	 char **authorizers, int numauthorizers)
{
    struct environment *en;
    int sessid, i, serrno;

    keynote_errno = 0;
    if ((sessid = kn_init()) == -1)
      return -1;

    /* Action set */
    for (en = env; en != (struct environment *) NULL; en = en->env_next)
      if (kn_add_action(sessid, en->env_name, en->env_value,
          en->env_flags) == -1)
      {
	  serrno = keynote_errno;
	  kn_close(sessid);
	  keynote_errno = serrno;
	  return -1;
      }

    /* Locally trusted assertions */
    for (i = 0; i < numtrusted; i++)
      if ((kn_add_assertion(sessid, trusted[i], trustedlen[i],
	  ASSERT_FLAG_LOCAL) == -1) && (keynote_errno == ERROR_MEMORY))
      {
	  serrno = keynote_errno;
	  kn_close(sessid);
	  keynote_errno = serrno;
	  return -1;
      }

    /* Untrusted assertions */
    for (i = 0; i < numuntrusted; i++)
      if ((kn_add_assertion(sessid, untrusted[i], untrustedlen[i], 0) == -1)
	  && (keynote_errno == ERROR_MEMORY))
      {
	  serrno = keynote_errno;
	  kn_close(sessid);
	  keynote_errno = serrno;
	  return -1;
      }

    /* Authorizers */
    for (i = 0; i < numauthorizers; i++)
      if (kn_add_authorizer(sessid, authorizers[i]) == -1)
      {
	  serrno = keynote_errno;
	  kn_close(sessid);
	  keynote_errno = serrno;
	  return -1;
      }

    i = kn_do_query(sessid, retvalues, numval);
    serrno = keynote_errno;
    kn_close(sessid);

    if (serrno)
      keynote_errno = serrno;

    return i;
}

/*
 * Read a buffer, break it up in assertions.
 */
char **
kn_read_asserts(char *buffer, int bufferlen, int *numassertions)
{
    int bufsize = 32, i, flag, valid;
    char **buf, **tempbuf, *ptr;

    keynote_errno = 0;
    if (buffer == (char *) NULL)
    {
	keynote_errno = ERROR_SYNTAX;
	return (char **) NULL;
    }

    buf = (char **) calloc(bufsize, sizeof(char *));
    if (buf == (char **) NULL)
    {
	keynote_errno = ERROR_MEMORY;
	return (char **) NULL;
    }

    /*
     * We'll go through the whole buffer looking for consecutive newlines,
     * which imply newline separation. We use the valid flag to keep
     * track of whether there may be an assertion after the last pair of
     * newlines, or whether there may be an assertion in the buffer to
     * begin with, if there are no consecutive newlines.
     */
    for (i = 0, flag = 0, valid = 0, *numassertions = 0, ptr = buffer;
	 i < bufferlen;
	 i++)
    {
	if (buffer[i] == '\n')
	{
	    if (flag)  /* Two newlines in a row, copy if there's anything */
	    {
		if (valid)  /* Something there */
		{
		    /* Allocate enough memory */
		    buf[*numassertions] = (char *) calloc((buffer + i) - ptr
							  + 1, sizeof(char));
		    if (buf[*numassertions] == (char *) NULL)
		    {
			/* Free any already-allocated strings */
			for (flag = 0; flag < *numassertions; flag++)
			  free(buf[flag]);
			free(buf);
			keynote_errno = ERROR_MEMORY;
			return (char **) NULL;
		    }

		    /* Copy string */
		    memcpy(buf[*numassertions], ptr, (buffer + i) - ptr);
		    (*numassertions)++;
		}

		valid = 0; /* Reset */
		flag = 0;
		ptr = buffer + i + 1; /* Point right after this newline */

		/* See if we need to resize the buffer */
		if (*numassertions > bufsize - 4)
		{
		    /* Allocate twice the space */
		    tempbuf = (char **) realloc(buf, 2 * bufsize *
						sizeof(char *));
		    if (tempbuf == (char **) NULL)
		    {
			for (flag = 0; flag < *numassertions; flag++)
			  free(buf[flag]);
			free(buf);
			keynote_errno = ERROR_MEMORY;
			return (char **) NULL;
		    }

		    free(buf);     /* Free old buffer */
		    buf = tempbuf;
		    bufsize *= 2;
		}
	    }
	    else
	      flag = 1;  /* One newline so far */

	    continue;
	}
	else
	  flag = 0;

	if (!isspace(buffer[i]))
	  valid = 1;
    }

    /*
     * There may be a valid assertion after the last pair of newlines.
     * Notice that because of the resizing check above, there will be
     * a valid memory location to store this last string.
     */
    if (valid)
    {
	/* This one's easy, we can just use strdup() */
	if ((buf[*numassertions] = strdup(ptr)) == (char *) NULL)
	{
	    for (flag = 0; flag < *numassertions; flag++)
	      free(buf[flag]);
	    free(buf);
	    keynote_errno = ERROR_MEMORY;
	    return (char **) NULL;
	}
	(*numassertions)++;
    }

    return buf;
}

/*
 * Return the authorizer key for a given assertion.
 */
void *
kn_get_authorizer(int sessid, int assertid, int *algorithm)
{
    struct assertion *as;
    int i;

    keynote_errno = 0;
    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return (void *) NULL;
	}
    }

    /* Traverse the hash table looking for assertid */
    for (i = 0; i < HASHTABLESIZE; i++)
      for (as = keynote_current_session->ks_assertion_table[i];
	   as != (struct assertion *) NULL;
	   as = as->as_next)
	if (as->as_id == assertid)
	  break;

    if (as == (struct assertion *) NULL)
    {
	keynote_errno = ERROR_NOTFOUND;
	return (void *) NULL;
    }

    *algorithm = as->as_signeralgorithm;
    return as->as_authorizer;
}

/*
 * Return the licensees for a given assertion.
 */
struct keynote_keylist *
kn_get_licensees(int sessid, int assertid)
{
    struct assertion *as;
    int i;

    keynote_errno = 0;
    if ((keynote_current_session == (struct keynote_session *) NULL) ||
	(keynote_current_session->ks_id != sessid))
    {
	keynote_current_session = keynote_find_session(sessid);
	if (keynote_current_session == (struct keynote_session *) NULL)
	{
	    keynote_errno = ERROR_NOTFOUND;
	    return (struct keynote_keylist *) NULL;
	}
    }

    /* Traverse the hash table looking for assertid */
    for (i = 0; i < HASHTABLESIZE; i++)
      for (as = keynote_current_session->ks_assertion_table[i];
	   as != (struct assertion *) NULL;
	   as = as->as_next)
	if (as->as_id == assertid)
	  break;

    if (as == (struct assertion *) NULL)
    {
	keynote_errno = ERROR_NOTFOUND;
	return (struct keynote_keylist *) NULL;
    }

    return (struct keynote_keylist *) as->as_keylist;
}
