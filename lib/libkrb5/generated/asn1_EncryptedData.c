/* Generated from /usr/src/lib/libkrb5/../../kerberosV/src/lib/asn1/k5.asn1 */
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
encode_EncryptedData(unsigned char *p, size_t len, const EncryptedData *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, &(data)->cipher, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->kvno)
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, (data)->kvno, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_ENCTYPE(p, len, &(data)->etype, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 0, &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_EncryptedData(const unsigned char *p, size_t len, EncryptedData *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
{
int dce_fix;
if((dce_fix = fix_dce(reallen, &len)) < 0)
return ASN1_BAD_FORMAT;
{
size_t newlen, oldlen;

e = der_match_tag (p, len, CONTEXT, CONS, 0, &l);
if (e)
return e;
else {
p += l;
len -= l;
ret += l;
e = der_get_length (p, len, &newlen, &l);
FORW;
{
int dce_fix;
oldlen = len;
if((dce_fix = fix_dce(newlen, &len)) < 0)return ASN1_BAD_FORMAT;
e = decode_ENCTYPE(p, len, &(data)->etype, &l);
FORW;
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}else 
len = oldlen - newlen;
}
}
}
{
size_t newlen, oldlen;

e = der_match_tag (p, len, CONTEXT, CONS, 1, &l);
if (e)
(data)->kvno = NULL;
else {
p += l;
len -= l;
ret += l;
e = der_get_length (p, len, &newlen, &l);
FORW;
{
int dce_fix;
oldlen = len;
if((dce_fix = fix_dce(newlen, &len)) < 0)return ASN1_BAD_FORMAT;
(data)->kvno = malloc(sizeof(*(data)->kvno));
if((data)->kvno == NULL) return ENOMEM;
e = decode_integer(p, len, (data)->kvno, &l);
FORW;
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}else 
len = oldlen - newlen;
}
}
}
{
size_t newlen, oldlen;

e = der_match_tag (p, len, CONTEXT, CONS, 2, &l);
if (e)
return e;
else {
p += l;
len -= l;
ret += l;
e = der_get_length (p, len, &newlen, &l);
FORW;
{
int dce_fix;
oldlen = len;
if((dce_fix = fix_dce(newlen, &len)) < 0)return ASN1_BAD_FORMAT;
e = decode_octet_string(p, len, &(data)->cipher, &l);
FORW;
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}else 
len = oldlen - newlen;
}
}
}
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}
}
if(size) *size = ret;
return 0;
fail:
free_EncryptedData(data);
return e;
}

void
free_EncryptedData(EncryptedData *data)
{
free_ENCTYPE(&(data)->etype);
if((data)->kvno) {
free((data)->kvno);
}
free_octet_string(&(data)->cipher);
}

size_t
length_EncryptedData(const EncryptedData *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_ENCTYPE(&(data)->etype);
ret += 1 + length_len(ret) + oldret;
}
if((data)->kvno){
int oldret = ret;
ret = 0;
ret += length_integer((data)->kvno);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_octet_string(&(data)->cipher);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_EncryptedData(const EncryptedData *from, EncryptedData *to)
{
if(copy_ENCTYPE(&(from)->etype, &(to)->etype)) return ENOMEM;
if((from)->kvno) {
(to)->kvno = malloc(sizeof(*(to)->kvno));
if((to)->kvno == NULL) return ENOMEM;
*((to)->kvno) = *((from)->kvno);
}else
(to)->kvno = NULL;
if(copy_octet_string(&(from)->cipher, &(to)->cipher)) return ENOMEM;
return 0;
}

