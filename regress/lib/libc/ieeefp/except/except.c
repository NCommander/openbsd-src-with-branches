/*	$OpenBSD: except.c,v 1.4 2003/07/31 21:48:03 deraadt Exp $	*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <ieeefp.h>
#include <float.h>

volatile sig_atomic_t signal_cought;

static volatile const double one  = 1.0;
static volatile const double zero = 0.0;
static volatile const double huge = DBL_MAX;
static volatile const double tiny = DBL_MIN;

static void
sigfpe(int signo)
{
	signal_cought = 1;
}

int
main(int argc, char *argv[])
{
	volatile double x;

	/*
	 * check to make sure that all exceptions are masked and 
	 * that the accumulated exception status is clear.
 	 */
	assert(fpgetmask() == 0);
	assert(fpgetsticky() == 0);

	/* set up signal handler */
	signal (SIGFPE, sigfpe);
	signal_cought = 0;

	/* trip divide by zero */
	x = one / zero;
	assert (fpgetsticky() & FP_X_DZ);
	assert (signal_cought == 0);
	fpsetsticky(0);

	/* trip invalid operation */
	x = zero / zero;
	assert (fpgetsticky() & FP_X_INV);
	assert (signal_cought == 0);
	fpsetsticky(0);

	/* trip overflow */
	x = huge * huge;
	assert (fpgetsticky() & FP_X_OFL);
	assert (signal_cought == 0);
	fpsetsticky(0);

	/* trip underflow */
	x = tiny * tiny;
	assert (fpgetsticky() & FP_X_UFL);
	assert (signal_cought == 0);
	fpsetsticky(0);

#if 0
	/* unmask and then trip divide by zero */
	fpsetmask(FP_X_DZ);
	x = one / zero;
	assert (signal_cought == 1);
	signal_cought = 0;

	/* unmask and then trip invalid operation */
	fpsetmask(FP_X_INV);
	x = zero / zero;
	assert (signal_cought == 1);
	signal_cought = 0;

	/* unmask and then trip overflow */
	fpsetmask(FP_X_OFL);
	x = huge * huge;
	assert (signal_cought == 1);
	signal_cought = 0;

	/* unmask and then trip underflow */
	fpsetmask(FP_X_UFL);
	x = tiny * tiny;
	assert (signal_cought == 1);
	signal_cought = 0;
#endif

	exit(0);
}

