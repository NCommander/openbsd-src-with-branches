/*

crc32.h

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1992 Tatu Ylonen, Espoo, Finland
                   All rights reserved

Created: Tue Feb 11 14:37:27 1992 ylo

Functions for computing 32-bit CRC.

*/

/* RCSID("$Id: crc32.h,v 1.2 1999/05/04 11:58:41 bg Exp $"); */

#ifndef CRC32_H
#define CRC32_H

/* This computes a 32 bit CRC of the data in the buffer, and returns the
   CRC.  The polynomial used is 0xedb88320. */
unsigned long crc32(const unsigned char *buf, unsigned int len);

#endif /* CRC32_H */
