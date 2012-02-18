/* $OpenBSD: rthread_stack.c,v 1.5 2011/11/06 11:48:59 guenther Exp $ */
/* $snafu: rthread_stack.c,v 1.12 2005/01/11 02:45:28 marc Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/types.h>
#include <sys/mman.h>

#include <machine/param.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "rthread.h"

/*
 * Follow uthread's example and keep around stacks that have default
 * attributes for possible reuse.
 */
static SLIST_HEAD(, stack) def_stacks = SLIST_HEAD_INITIALIZER(head);
static _spinlock_lock_t def_stacks_lock = _SPINLOCK_UNLOCKED;

struct stack *
_rthread_alloc_stack(pthread_t thread)
{
	struct stack *stack;
	caddr_t base;
	caddr_t guard;
	size_t size;
	size_t guardsize;

	/* if the request uses the defaults, try to reuse one */
	if (thread->attr.stack_addr != NULL &&
	    thread->attr.stack_size == RTHREAD_STACK_SIZE_DEF &&
	    thread->attr.guard_size == _rthread_attr_default.guard_size) {
		_spinlock(&def_stacks_lock);
		stack = SLIST_FIRST(&def_stacks);
		if (stack != NULL)
		_spinunlock(&def_stacks_lock);
		if (stack != NULL)
			return (stack);
	}

	/* allocate the stack struct that we'll return */
	stack = malloc(sizeof(*stack));
	if (stack == NULL)
		return (NULL);

	/* If a stack address was provided, just fill in the details */
	if (thread->attr.stack_addr != NULL) {
		stack->base = thread->attr.stack_addr;
		stack->len  = thread->attr.stack_size;
#ifdef MACHINE_STACK_GROWS_UP
		stack->sp = thread->attr.stack_addr;
#else
		stack->sp = thread->attr.stack_addr + thread->attr.stack_size;
#endif
		/*
		 * This impossible guardsize marks this stack as
		 * application allocated so it won't be freed or
		 * cached by _rthread_free_stack()
		 */
		stack->guardsize = 1;
		return (stack);
	}

	/* round up the requested sizes up to full pages */
	size = ROUND_TO_PAGE(thread->attr.stack_size);
	guardsize = ROUND_TO_PAGE(thread->attr.guard_size);

	/* check for overflow */
	if (size < thread->attr.stack_size ||
	    guardsize < thread->attr.guard_size ||
	    SIZE_MAX - size < guardsize) {
		free(stack);
		errno = EINVAL;
		return (NULL);
	}
	size += guardsize;

	/* actually allocate the real stack */
	base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	if (base == MAP_FAILED) {
		free(stack);
		return (NULL);
	}

#ifdef MACHINE_STACK_GROWS_UP
	guard = base + size - guardsize;
	stack->sp = base;
#else
	guard = base;
	stack->sp = base + size;
#endif

	/* memory protect the guard region */
	if (guardsize != 0 && mprotect(guard, guardsize, PROT_NONE) == -1) {
		munmap(base, size);
		free(stack);
		return (NULL);
	}

	stack->base = base;
	stack->guardsize = guardsize;
	stack->len = size;
	return (stack);
}

void
_rthread_free_stack(struct stack *stack)
{
	if (stack->len == RTHREAD_STACK_SIZE_DEF &&
	    stack->guardsize == _rthread_attr_default.guard_size) {
		_spinlock(&def_stacks_lock);
		SLIST_INSERT_HEAD(&def_stacks, stack, link);
		_spinunlock(&def_stacks_lock);
	} else {
		/* unmap the storage unless it was application allocated */
		if (stack->guardsize != 1)
			munmap(stack->base, stack->len);
		free(stack);
	}
}

