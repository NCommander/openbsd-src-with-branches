/*	$OpenBSD: fenv.c,v 1.1 2011/04/23 22:39:14 martynas Exp $	*/

/*
 * Copyright (c) 2011 Martynas Venckus <martynas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <machine/sysarch.h>

#include <fenv.h>

/*
 * The following constant represents the default floating-point environment
 * (that is, the one installed at program startup) and has type pointer to
 * const-qualified fenv_t.
 *
 * It can be used as an argument to the functions within the <fenv.h> header
 * that manage the floating-point environment, namely fesetenv() and
 * feupdateenv().
 */
fenv_t __fe_dfl_env = {
	0,
	0,
	FE_TONEAREST
};

/*
 * The feclearexcept() function clears the supported floating-point exceptions
 * represented by `excepts'.
 */
int
feclearexcept(int excepts)
{
	unsigned int fpsticky;
	struct alpha_fp_except_args a;

	excepts &= FE_ALL_EXCEPT;

	/* Store the current floating-point sticky flags */
	fpsticky = sysarch(ALPHA_FPGETSTICKY, 0L);

	/* Clear the requested floating-point exceptions */
	fpsticky &= ~excepts;

	/* Load the floating-point sticky flags */
	a.mask = fpsticky;
	sysarch(ALPHA_FPSETSTICKY, &a);

	return (0);
}

/*
 * The fegetexceptflag() function stores an implementation-defined
 * representation of the states of the floating-point status flags indicated by
 * the argument excepts in the object pointed to by the argument flagp.
 */
int
fegetexceptflag(fexcept_t *flagp, int excepts)
{
	unsigned int fpsticky;

	excepts &= FE_ALL_EXCEPT;

	/* Store the current floating-point sticky flags */
	fpsticky = sysarch(ALPHA_FPGETSTICKY, 0L);

	/* Store the results in flagp */
	*flagp = fpsticky & excepts;

	return (0);
}

/*
 * The feraiseexcept() function raises the supported floating-point exceptions
 * represented by the argument `excepts'.
 */
int
feraiseexcept(int excepts)
{
	excepts &= FE_ALL_EXCEPT;

	fesetexceptflag((fexcept_t *)&excepts, excepts);

	return (0);
}

/*
 * This function sets the floating-point status flags indicated by the argument
 * `excepts' to the states stored in the object pointed to by `flagp'. It does
 * NOT raise any floating-point exceptions, but only sets the state of the flags.
 */
int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	unsigned int fpsticky;
	struct alpha_fp_except_args a;

	excepts &= FE_ALL_EXCEPT;

	/* Store the current floating-point sticky flags */
	fpsticky = sysarch(ALPHA_FPGETSTICKY, 0L);

	/* Set the requested status flags */
	fpsticky &= ~excepts;
	fpsticky |= *flagp & excepts;

	/* Load the floating-point sticky flags */
	a.mask = fpsticky;
	sysarch(ALPHA_FPSETSTICKY, &a);

	return (0);
}

/*
 * The fetestexcept() function determines which of a specified subset of the
 * floating-point exception flags are currently set. The `excepts' argument
 * specifies the floating-point status flags to be queried.
 */
int
fetestexcept(int excepts)
{
	unsigned int fpsticky;

	excepts &= FE_ALL_EXCEPT;

	/* Store the current floating-point sticky flags */
	fpsticky = sysarch(ALPHA_FPGETSTICKY, 0L);

	return (fpsticky & excepts);
}

/*
 * The fegetround() function gets the current rounding direction.
 */
int
fegetround(void)
{
	unsigned long fpcr;

	/* Store the current floating-point control register */
	__asm__ __volatile__ ("trapb");
	__asm__ __volatile__ ("mf_fpcr %0" : "=f" (fpcr));
	__asm__ __volatile__ ("trapb");

	return ((fpcr >> _ROUND_SHIFT) & _ROUND_MASK);
}

/*
 * The fesetround() function establishes the rounding direction represented by
 * its argument `round'. If the argument is not equal to the value of a rounding
 * direction macro, the rounding direction is not changed.
 */
int
fesetround(int round)
{
	unsigned long fpcr;

	/* Check whether requested rounding direction is supported */
	if (round & ~_ROUND_MASK)
		return (-1);

	/* Store the current floating-point control register */
	__asm__ __volatile__ ("trapb");
	__asm__ __volatile__ ("mf_fpcr %0" : "=f" (fpcr));
	__asm__ __volatile__ ("trapb");

	/* Set the rounding direction */
	fpcr &= ~((unsigned long)_ROUND_MASK << _ROUND_SHIFT);
	fpcr |= (unsigned long)round << _ROUND_SHIFT;

	/* Load the floating-point control register */
	__asm__ __volatile__ ("trapb");
	__asm__ __volatile__ ("mt_fpcr %0" : : "f" (fpcr));
	__asm__ __volatile__ ("trapb");

	return (0);
}

/*
 * The fegetenv() function attempts to store the current floating-point
 * environment in the object pointed to by envp.
 */
int
fegetenv(fenv_t *envp)
{
	unsigned long fpcr;

	/* Store the current floating-point sticky flags */
	envp->__sticky = sysarch(ALPHA_FPGETSTICKY, 0L) & FE_ALL_EXCEPT;

	/* Store the current floating-point masks */
	envp->__mask = sysarch(ALPHA_FPGETMASK, 0L) & FE_ALL_EXCEPT;

	/* Store the current floating-point control register */
	__asm__ __volatile__ ("trapb");
	__asm__ __volatile__ ("mf_fpcr %0" : "=f" (fpcr));
	__asm__ __volatile__ ("trapb");
	envp->__round = (fpcr >> _ROUND_SHIFT) & _ROUND_MASK;

	return (0);
}

/*
 * The feholdexcept() function saves the current floating-point environment
 * in the object pointed to by envp, clears the floating-point status flags, and
 * then installs a non-stop (continue on floating-point exceptions) mode, if
 * available, for all floating-point exceptions.
 */
int
feholdexcept(fenv_t *envp)
{
	struct alpha_fp_except_args a;

	/* Store the current floating-point environment */
	fegetenv(envp);

	/* Clear exception flags */
	a.mask = 0;
	sysarch(ALPHA_FPSETSTICKY, &a);

	/* Mask all exceptions */
	a.mask = 0;
	sysarch(ALPHA_FPSETMASK, &a);

	return (0);
}

/*
 * The fesetenv() function attempts to establish the floating-point environment
 * represented by the object pointed to by envp. The argument `envp' points
 * to an object set by a call to fegetenv() or feholdexcept(), or equal a
 * floating-point environment macro. The fesetenv() function does not raise
 * floating-point exceptions, but only installs the state of the floating-point
 * status flags represented through its argument.
 */
int
fesetenv(const fenv_t *envp)
{
	unsigned long fpcr;
	struct alpha_fp_except_args a;

	/* Load the floating-point sticky flags */
	a.mask = envp->__sticky & FE_ALL_EXCEPT;
	sysarch(ALPHA_FPSETSTICKY, &a);

	/* Load the floating-point masks */
	a.mask = envp->__mask & FE_ALL_EXCEPT;
	sysarch(ALPHA_FPSETMASK, &a);

	/* Store the current floating-point control register */
	__asm__ __volatile__ ("trapb");
	__asm__ __volatile__ ("mf_fpcr %0" : "=f" (fpcr));
	__asm__ __volatile__ ("trapb");

	/* Set the requested flags */
	fpcr &= ~((unsigned long)_ROUND_MASK << _ROUND_SHIFT);
	fpcr |= ((unsigned long)envp->__round & _ROUND_MASK) << _ROUND_SHIFT;

	/* Load the floating-point control register */
	__asm__ __volatile__ ("trapb");
	__asm__ __volatile__ ("mt_fpcr %0" : : "f" (fpcr));
	__asm__ __volatile__ ("trapb");

	return (0);
}

/*
 * The feupdateenv() function saves the currently raised floating-point
 * exceptions in its automatic storage, installs the floating-point environment
 * represented by the object pointed to by `envp', and then raises the saved
 * floating-point exceptions. The argument `envp' shall point to an object set
 * by a call to feholdexcept() or fegetenv(), or equal a floating-point
 * environment macro.
 */
int
feupdateenv(const fenv_t *envp)
{
	unsigned int fpsticky;

	/* Store the current floating-point sticky flags */
	fpsticky = sysarch(ALPHA_FPGETSTICKY, 0L);

	/* Install new floating-point environment */
	fesetenv(envp);

	/* Raise any previously accumulated exceptions */
	feraiseexcept(fpsticky);

	return (0);
}

/*
 * The following functions are extentions to the standard
 */
int
feenableexcept(int mask)
{
	unsigned int fpmask, omask;
	struct alpha_fp_except_args a;

	mask &= FE_ALL_EXCEPT;

	/* Store the current floating-point masks */
	fpmask = sysarch(ALPHA_FPGETMASK, 0L);

	omask = fpmask & FE_ALL_EXCEPT;
	fpmask |= mask;

	/* Load the floating-point masks */
	a.mask = fpmask;
	sysarch(ALPHA_FPSETMASK, &a);

	return (omask);

}

int
fedisableexcept(int mask)
{
	unsigned int fpmask, omask;
	struct alpha_fp_except_args a;

	mask &= FE_ALL_EXCEPT;

	/* Store the current floating-point masks */
	fpmask = sysarch(ALPHA_FPGETMASK, 0L);

	omask = fpmask & FE_ALL_EXCEPT;
	fpmask &= ~mask;

	/* Load the floating-point masks */
	a.mask = fpmask;
	sysarch(ALPHA_FPSETMASK, &a);

	return (omask);
}

int
fegetexcept(void)
{
	unsigned int fpmask;

	/* Store the current floating-point masks */
	fpmask = sysarch(ALPHA_FPGETMASK, 0L);

	return (fpmask & FE_ALL_EXCEPT);
}
