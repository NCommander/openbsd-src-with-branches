/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_IDSTR(id, "@(#)$Sendmail: t-fopen.c,v 1.6 2001/03/05 03:21:28 ca Exp $")

#include <fcntl.h>
#include <sm/io.h>
#include <sm/test.h>

/* ARGSUSED0 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	SM_FILE_T *out;

	sm_test_begin(argc, argv, "test sm_io_fopen");
	out = sm_io_fopen("foo", O_WRONLY|O_APPEND|O_CREAT, 0666);
	SM_TEST(out != NULL);
	if (out != NULL)
	{
		(void) sm_io_fprintf(out, SM_TIME_DEFAULT, "foo\n");
		sm_io_close(out, SM_TIME_DEFAULT);
	}
	return sm_test_end();
}
