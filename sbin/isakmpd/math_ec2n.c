/*	$Id: math_ec2n.c,v 1.4 1998/07/18 21:09:41 provos Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
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
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/param.h>
#include <stdio.h>

#include "math_2n.h"
#include "math_ec2n.h"

void
ec2np_init (ec2np_ptr n)
{
  b2n_init (n->x);
  b2n_init (n->y);
  n->inf = 0;
}

void
ec2np_clear (ec2np_ptr n)
{
  b2n_clear (n->x);
  b2n_clear (n->y);
}

void
ec2np_set (ec2np_ptr d, ec2np_ptr n)
{
  if (d == n)
    return;

  d->inf = n->inf;
  b2n_set (d->x, n->x);
  b2n_set (d->y, n->y);
}

/* Group */

void
ec2ng_init (ec2ng_ptr n)
{
  b2n_init (n->a);
  b2n_init (n->b);
  b2n_init (n->p);
}

void
ec2ng_clear (ec2ng_ptr n)
{
  b2n_clear (n->a);
  b2n_clear (n->b);
  b2n_clear (n->p);
}

void
ec2ng_set (ec2ng_ptr d, ec2ng_ptr n)
{
  b2n_set (d->a, n->a);
  b2n_set (d->b, n->b);
  b2n_set (d->p, n->p);
}

/* Arithmetic functions */

void
ec2np_right (b2n_ptr n, ec2np_ptr p, ec2ng_ptr g)
{
  b2n_t temp;

  b2n_init (temp);
  /* First calc x**3 + ax**2 + b */
  b2n_square (n, p->x);
  b2n_mod (n, n, g->p);

  b2n_mul (temp, g->a, n);	/* a*x**2 */
  b2n_mod (temp, temp, g->p);

  b2n_mul (n, n, p->x);		/* x**3 */
  b2n_mod (n, n, g->p);

  b2n_add (n, n, temp);
  b2n_add (n, n, g->b);

  b2n_clear (temp);
}

int
ec2np_ison (ec2np_ptr p, ec2ng_ptr g)
{
  int res;

  b2n_t x, y, temp;
  
  if (p->inf)
    return 1;

  b2n_init (x);
  b2n_init (y);
  b2n_init (temp);

  /* First calc x**3 + ax**2 + b */
  ec2np_right (x, p, g);

  /* Now calc y**2 + xy */
  b2n_square (y, p->y);
  b2n_mod (y, y, g->p);

  b2n_mul (temp, p->y, p->x);
  b2n_mod (temp, temp, g->p);

  b2n_add (y, y, temp);

  res = !b2n_cmp (x, y);

  b2n_clear (x);
  b2n_clear (y);
  b2n_clear (temp);

  return res;
}

int
ec2np_find_y (ec2np_ptr p, ec2ng_ptr g)
{
  b2n_t right;

  b2n_init (right);
  ec2np_right (right, p, g);		/* Right sight of equation */
  b2n_mul_inv (p->y, p->x, g->p);

  b2n_square (p->y, p->y);
  b2n_mod (p->y, p->y, g->p);

  b2n_mul (right, right, p->y);		/* x^-2 * right */
  b2n_mod (right, right, g->p);

  b2n_sqrt (p->y, right, g->p);		/* Find root */
  b2n_mul (p->y, p->y, p->x);
  b2n_mod (p->y, p->y, g->p);

  return 1;
}

void
ec2np_add (ec2np_ptr d, ec2np_ptr a, ec2np_ptr b, ec2ng_ptr g)
{
  b2n_t lambda, temp;
  ec2np_t pn;

  /* Check for Neutral Element */
  if (b->inf)
    {
      ec2np_set (d, a);
      return;
    }
  if (a->inf)
    {
      ec2np_set (d, b);
      return;
    }

  if (!b2n_cmp (a->x, b->x) && (b2n_cmp (a->y, b->y) || !b2n_cmp_null (a->x)))
    {
      d->inf = 1;
      b2n_set_null (d->x);
      b2n_set_null (d->y);
      return;
    }

  b2n_init (lambda);
  b2n_init (temp);
  ec2np_init (pn);

  if (b2n_cmp (a->x, b->x))
    {
      b2n_add (temp, a->x, b->x);
      b2n_add (lambda, a->y, b->y);
      b2n_div_mod (lambda, lambda, temp, g->p);

      b2n_square (pn->x, lambda);
      b2n_mod (pn->x, pn->x, g->p);

      b2n_add (pn->x, pn->x, lambda);
      b2n_add (pn->x, pn->x, g->a);
      b2n_add (pn->x, pn->x, a->x);
      b2n_add (pn->x, pn->x, b->x);
    }
  else
    {
      b2n_div_mod (lambda, b->y, b->x, g->p);
      b2n_add (lambda, lambda, b->x);

      b2n_square (pn->x, lambda);
      b2n_mod (pn->x, pn->x, g->p);
      b2n_add (pn->x, pn->x, lambda);
      b2n_add (pn->x, pn->x, g->a);
    }

  b2n_add (pn->y, b->x, pn->x);

  b2n_mul (pn->y, pn->y, lambda);
  b2n_mod (pn->y, pn->y, g->p);

  b2n_add (pn->y, pn->y, pn->x);
  b2n_add (pn->y, pn->y, b->y);

  EC2NP_SWAP (d, pn);

  ec2np_clear (pn);
  b2n_clear (lambda);
  b2n_clear (temp);
}

void
ec2np_mul (ec2np_ptr d, ec2np_ptr a, b2n_ptr e, ec2ng_ptr g)
{
  int i, j, bits, start;
  b2n_t h, k;
  ec2np_t q, mina;

  if (!b2n_cmp_null (e))
    {
      d->inf = 1;
      b2n_set_null (d->x);
      b2n_set_null (d->y);
      return;
    }

  b2n_init (h);
  b2n_init (k);

  ec2np_init (q);
  ec2np_init (mina);
  ec2np_set (q, a);

  /* Create the point -a */
  ec2np_set (mina, a);
  b2n_add (mina->y, mina->y, mina->x);

  b2n_set (k, e);
  b2n_3mul (h, k);
  b2n_resize (k, h->chunks);

  /* 
   * This is low level but can not be avoided, since we have to do single
   * bit checks on h and k.
   */
  bits = b2n_sigbit (h);
  if ((bits & CHUNK_MASK) == 1)
    {
      start = ((CHUNK_MASK + bits) >> CHUNK_SHIFTS) - 2;
      bits = CHUNK_BITS;
    }
  else
    {
      start = ((CHUNK_MASK + bits) >> CHUNK_SHIFTS) - 1;
      bits = ((bits - 1) & CHUNK_MASK);
    }

  /* 
   * This is the addition, subtraction method which is faster because
   * we avoid one out of three additions (mean).
   */

  for (i = start; i >= 0; i--)
    for (j = (i == start ? bits : CHUNK_BITS) - 1; j >= 0; j--)
      if (i > 0 || j > 0)
	{
	  ec2np_add (q, q, q, g);
	  if ((h->limp[i] & b2n_mask[j]) && !(k->limp[i] & b2n_mask[j]))
	    ec2np_add (q, q, a, g);
	  else if (!(h->limp[i] & b2n_mask[j]) && (k->limp[i] & b2n_mask[j]))
	    ec2np_add (q, q, mina, g);
	}

  EC2NP_SWAP (d, q);

  b2n_clear (k);
  b2n_clear (h);

  ec2np_clear (q);
  ec2np_clear (mina);
}
