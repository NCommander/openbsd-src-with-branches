/* $Id: cyberflex.c,v 1.5 2001/06/27 22:33:35 rees Exp $ */

/*
copyright 2000
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works
and redistribute this software and such derivative works
for any purpose, so long as the name of the university of
michigan is not used in any advertising or publicity
pertaining to the use or distribution of this software
without specific, written prior authorization.  if the
above copyright notice or any other identification of the
university of michigan is included in any copy of any
portion of this software, then the disclaimer below must
also be included.

this software is provided as is, without representation
from the university of michigan as to its fitness for any
purpose, and without warranty by the university of
michigan of any kind, either express or implied, including
without limitation the implied warranties of
merchantability and fitness for a particular purpose. the
regents of the university of michigan shall not be liable
for any damages, including special, indirect, incidental, or
consequential damages, with respect to any claim arising
out of or in connection with the use of the software, even
if it has been or is hereafter advised of the possibility of
such damages.
*/

/*
 * Cyberflex routines
 *
 * University of Michigan CITI, July 2001
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#ifdef __linux
#include <openssl/des.h>
#else /* __linux */
#include <des.h>
#endif

#include "sectok.h"

#define MAX_APDU_SIZE 0xfa
#define MAX_KEY_FILE_SIZE 1024
#define PRV_KEY_SIZE 64*6
#define key_number 0x10
#define key_type 0xc8 /* key type 0xc8 (1024 bit RSA private) */
#define KEY_FILE_HEADER_SIZE 8
#define BLOCK_SIZE 8

int cyberflex_create_file(int fd, int cla, unsigned char *fid, int size, int ftype,
			  int *r1p, int *r2p)
{
    int i, n;
    unsigned char data[16];

    size += 16;

    data[0] = (size >> 8);
    data[1] = (size & 0xff);
    data[2] = fid[0];
    data[3] = fid[1];
    data[4] = ftype;
    data[5] = 0x01;		/* status = 1 */
    data[6] = data[7] = 0x00;	/* record related */
    data[8] = 0xff;		/* ACL can do everything with AUT0 */
    for (i = 9; i < 16; i++ )
	data[i] = 0x00;		/* ACL : cannot do anything without AUT0 */

    n = scwrite(fd, cla, 0xe0, 0, 0, 0x10, data, r1p, r2p);
    if (n < 0 || (*r1p != 0x90 && *r1p != 0x61))
	return -1;

    return sectok_selectfile(fd, cla, fid, r1p, r2p);
}

int
cyberflex_delete_file(int fd, int cla, int f0, int f1, int *r1p, int *r2p)
{
    int n;
    unsigned char buf[2];

    buf[0] = f0;
    buf[1] = f1;

    n = scwrite(fd, cla, 0xe4, 0, 0, 0x02, buf, r1p, r2p);
    if (n < 0 || (*r1p != 0x90 && *r1p != 0x61)) {
	/* error */
	return -1;
    }
    return 0;
}

int
cyberflex_load_rsa_pub(int fd, int cla, unsigned char *key_fid,
		       int key_len, unsigned char *key_data, int *r1p, int *r2p)
{
    int rv;

    if (sectok_selectfile(fd, cla, root_fid, r1p, r2p) < 0)
	return -1;

    if (sectok_selectfile(fd, cla, key_fid, r1p, r2p)) {
	if (cyberflex_create_file(fd, cla, key_fid, key_len, 3, r1p, r2p) < 0)
	    return -1;
    }

    /* Write the key data */
    rv = scwrite(fd, cla, 0xd6, 0, 0, key_len, key_data, r1p, r2p);
    if (rv < 0 || (*r1p != 0x90 && *r1p != 0x61))
	return -1;

    return rv;
}

/* download RSA private key into 3f.00/00.12 */
int
cyberflex_load_rsa_priv(int fd, int cla, unsigned char *key_fid,
			int nkey_elems, int key_len, unsigned char *key_elems[],
			int *r1p, int *r2p)
{
    int i, j, rv, offset = 0, size;
    unsigned char data[MAX_KEY_FILE_SIZE];
    static unsigned char key_file_header[KEY_FILE_HEADER_SIZE] =
    {0xC2, 0x06, 0xC1, 0x08, 0x13, 0x00, 0x00, 0x05};
    static unsigned char key_header[3] = {0xC2, 0x41, 0x00};

    /* select 3f.00 */
    rv = sectok_selectfile(fd, cla, root_fid, r1p, r2p);
    if (rv < 0) return rv;

    /* select 00.12 */
    rv = sectok_selectfile(fd, cla, key_fid, r1p, r2p);
    if (rv < 0) {
	/* rv != 0, 00.12 does not exist.  create it. */
	printf ("private key file does not exist.  create it.\n");
	if (cyberflex_create_file(fd, cla, key_fid, PRV_KEY_SIZE, 3, r1p, r2p) < 0)
	    return -1;
    }

    /* burn the key */
    data[0] = 0x01;		/* key size, I guess */
    data[1] = 0x5b;		/* key size, I guess */
    data[2] = key_number;	/* key number */
    data[3] = key_type;
    offset = 4;
    for (j = 0 ; j < KEY_FILE_HEADER_SIZE ; j ++)
	data[offset++] = key_file_header[j];
    for (i = 0 ; i < nkey_elems; i ++) {
	/* put the key header */
	for (j = 0 ; j < 3 ; j ++) {
	    data[offset++] = key_header[j];
	}
	for (j = 0 ; j < key_len/2/8 ; j ++) {
	    data[offset++] = key_elems [i][j];
	}
    }
    for (j = 0 ; j < 2 ; j ++) data[offset++] = 0;

#ifdef DEBUG
    printf ("data:\n");
    for (i = 0 ; i < 0x015d; i ++) {
	printf ("%02x ", data[i]);
    }
    printf ("\n");
#endif

    /* now send this to the card */
    /* select private key file */
    if (sectok_selectfile(fd, cla, key_fid, r1p, r2p) < 0)
	return -1;

    /* update binary */
    size = offset;

    for (i = 0; i < size; i += MAX_APDU_SIZE) {
	int send_size;

	/* compute the size to be sent */
	if (size - i > MAX_APDU_SIZE) send_size = MAX_APDU_SIZE;
	else send_size = size - i;

	rv = scwrite(fd, cla, 0xd6,
		     i / 256,	/* offset, upper byte */
		     i % 256,	/* offset, lower byte */
		     send_size,
		     data + i,	/* key file */
		     r1p, r2p);

	if (*r1p != 0x90 && *r1p != 0x61)
	    return -1;
    }

    printf ("rsa key loading done! :)\n");
    return 0;
}

int
cyberflex_verify_AUT0(int fd, int cla, unsigned char *aut0, int aut0len)
{
    int n, r1, r2;

    n = scwrite(fd, cla, 0x2a, 0, 0, aut0len, aut0, &r1, &r2);
    if (n < 0 || r1 != 0x90) {
	if (n >= 0)
	    print_r1r2(r1, r2);
	return -1;
    }
    return 0;
}

/* fill the key block.

   Input
   dst     : destination buffer
   key_num : key number (0: AUT, 5: signed applet, etc.)
   alg_num : algorithm number
   key     : incoming 8 byte DES key

   The resulting format:
   00 0e key_num alg_num key(8 byte) 0a 0a

   total 14 byte
*/
void
cyberflex_fill_key_block (unsigned char *dst, int key_num,
			       int alg_num, unsigned char *key)
{
    int i;

    *(dst+0) = 0x00;		/* const */
    *(dst+1) = 0x0e;		/* const */
    *(dst+2) = key_num;		/* key number */
    *(dst+3) = alg_num;		/* algorithm number */
    for (i = 0; i < BLOCK_SIZE; i++)
	*(dst+i+4) = *(key+i);
    *(dst+12) = 0x0a;		/* const */
    *(dst+13) = 0x0a;		/* const */

    return;
}

int
cyberflex_inq_class(int fd)
{
    unsigned char buf[32];
    int n, r1, r2;

    n = scread(fd, 0x00, 0xca, 0, 1, 0x16, buf, &r1, &r2);
    if (n >= 0 && r1 == 0x90)
	return 0x00;

    if (n >= 0 && r1 == 0x6d) {
        /* F0 card? */
        n = scread(fd, 0xf0, 0xca, 0, 1, 0x16, buf, &r1, &r2);
        if (n >= 0 && r1 == 0x90)
	    return 0xf0;
    }

    return -1;
}
