/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <krb5_asn1.h>
#include <asn1_err.h>
#include <der.h>
#include <parse_units.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_AD_MANDATORY_FOR_KDC(unsigned char *p, size_t len, const AD_MANDATORY_FOR_KDC *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
e = encode_AuthorizationData(p, len, data, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_AD_MANDATORY_FOR_KDC(const unsigned char *p, size_t len, AD_MANDATORY_FOR_KDC *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = decode_AuthorizationData(p, len, data, &l);
FORW;
if(size) *size = ret;
return 0;
fail:
free_AD_MANDATORY_FOR_KDC(data);
return e;
}

void
free_AD_MANDATORY_FOR_KDC(AD_MANDATORY_FOR_KDC *data)
{
free_AuthorizationData(data);
}

size_t
length_AD_MANDATORY_FOR_KDC(const AD_MANDATORY_FOR_KDC *data)
{
size_t ret = 0;
ret += length_AuthorizationData(data);
return ret;
}

int
copy_AD_MANDATORY_FOR_KDC(const AD_MANDATORY_FOR_KDC *from, AD_MANDATORY_FOR_KDC *to)
{
if(copy_AuthorizationData(from, to)) return ENOMEM;
return 0;
}

