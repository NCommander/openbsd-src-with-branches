/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Sendmail: shm.c,v 1.8 2001/09/11 04:04:49 gshapiro Exp $")

#if SM_CONF_SHM
# include <stdlib.h>
# include <unistd.h>
# include <errno.h>
# include <sm/shm.h>

/*
**  SM_SHMSTART -- initialize shared memory segment.
**
**	Parameters:
**		key -- key for shared memory.
**		size -- size of segment.
**		shmflag -- initial flags.
**		shmid -- pointer to return id.
**		owner -- create segment.
**
**	Returns:
**		pointer to shared memory segment,
**		NULL on failure.
**
**	Side Effects:
**		attaches shared memory segment.
*/

void *
sm_shmstart(key, size, shmflg, shmid, owner)
	key_t key;
	int size;
	int shmflg;
	int *shmid;
	bool owner;
{
	int save_errno;
	void *shm = SM_SHM_NULL;

	/* default: user/group accessible */
	if (shmflg == 0)
		shmflg = SHM_R|SHM_W|(SHM_R>>3)|(SHM_W>>3);
	if (owner)
		shmflg |= IPC_CREAT|IPC_EXCL;
	*shmid = shmget(key, size, shmflg);
	if (*shmid < 0)
		goto error;

	shmflg = SHM_RND;
	shm = shmat(*shmid, (void *) 0, shmflg);
	if (shm == SM_SHM_NULL)
		goto error;

	return shm;

  error:
	save_errno = errno;
	if (shm != SM_SHM_NULL || *shmid >= 0)
		sm_shmstop(shm, *shmid, owner);
	*shmid = SM_SHM_NO_ID;
	errno = save_errno;
	return (void *) 0;
}

/*
**  SM_SHMSTOP -- stop using shared memory segment.
**
**	Parameters:
**		shm -- pointer to shared memory.
**		shmid -- id.
**		owner -- delete segment.
**
**	Returns:
**		0 on success.
**		< 0 on failure.
**
**	Side Effects:
**		detaches (and maybe removes) shared memory segment.
*/

int
sm_shmstop(shm, shmid, owner)
	void *shm;
	int shmid;
	bool owner;
{
	int r;

	if (shm != SM_SHM_NULL && (r = shmdt(shm)) < 0)
		return r;
	if (owner && shmid >= 0 && (r = shmctl(shmid, IPC_RMID, NULL)) < 0)
		return r;
	return 0;
}
#endif /* SM_CONF_SHM */
