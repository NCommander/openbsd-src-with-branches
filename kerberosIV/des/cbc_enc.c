/*	$Id$	*/

/* Copyright (C) 1993 Eric Young - see README for more details */
#include "des_locl.h"

int des_cbc_encrypt(des_cblock (*input), des_cblock (*output), long int length, struct des_ks_struct *schedule, des_cblock (*ivec), int encrypt)
{
  register u_int32_t tin0,tin1;
  register u_int32_t tout0,tout1,xor0,xor1;
  register unsigned char *in,*out;
  register long l=length;
  u_int32_t tout[2],tin[2];
  unsigned char *iv;

  in=(unsigned char *)input;
  out=(unsigned char *)output;
  iv=(unsigned char *)ivec;

  if (encrypt)
    {
      c2l(iv,tout0);
      c2l(iv,tout1);
      for (; l>0; l-=8)
	{
	  if (l >= 8)
	    {
	      c2l(in,tin0);
	      c2l(in,tin1);
	    }
	  else
	    c2ln(in,tin0,tin1,l);
	  tin0^=tout0;
	  tin1^=tout1;
	  tin[0]=tin0;
	  tin[1]=tin1;
	  des_encrypt(tin,tout,
		      schedule,encrypt);
	  tout0=tout[0];
	  tout1=tout[1];
	  l2c(tout0,out);
	  l2c(tout1,out);
	}
    }
  else
    {
      c2l(iv,xor0);
      c2l(iv,xor1);
      for (; l>0; l-=8)
	{
	  c2l(in,tin0);
	  c2l(in,tin1);
	  tin[0]=tin0;
	  tin[1]=tin1;
	  des_encrypt(tin,tout,
		      schedule,encrypt);
	  tout0=tout[0]^xor0;
	  tout1=tout[1]^xor1;
	  if (l >= 8)
	    {
	      l2c(tout0,out);
	      l2c(tout1,out);
	    }
	  else
	    l2cn(tout0,tout1,out,l);
	  xor0=tin0;
	  xor1=tin1;
	}
    }
  tin0=tin1=tout0=tout1=xor0=xor1=0;
  tin[0]=tin[1]=tout[0]=tout[1]=0;
  return(0);
}

