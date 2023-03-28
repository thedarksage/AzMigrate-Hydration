/*
 * Copyright (C) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : bitmap_operations.h
 *
 * Description: This file contains data mode implementation of the
 *              filter driver.
 *
 * Functions defined are
 *
 *
 */

#ifndef _INMAGE_BITMAP_OPERATIONS_H
#define _INMAGE_BITMAP_OPERATIONS_H

#include "involflt-common.h"

inm_u32_t find_number_of_bits_set(unsigned char *bit_buffer,
				 inm_u32_t buffer_size_in_bits);

inm_s32_t SetBitmapBitRun(unsigned char * bitBuffer,
                    inm_u32_t bitsInBitmap,
                    inm_u32_t bitsInRun,
                    inm_u32_t bitOffset,
                    inm_u32_t * nbrBytesChanged,
                    unsigned char * *firstByteChanged);
inm_s32_t ClearBitmapBitRun(unsigned char * bitBuffer,
		      inm_u32_t bitsInBitmap,
		      inm_u32_t bitsInRun,
		      inm_u32_t bitOffset,
		      inm_u32_t * nbrBytesChanged,
		      unsigned char * *firstByteChanged);

inm_s32_t InvertBitmapBitRun(unsigned char * bitBuffer,
		       inm_u32_t bitsInBitmap,
		       inm_u32_t bitsInRun,
		       inm_u32_t bitOffset,
		       inm_u32_t * nbrBytesChanged,
		       unsigned char * *firstByteChanged);

inm_s32_t GetNextBitmapBitRun(
    unsigned char * bitBuffer,
    inm_u32_t totalBitsInBitmap,
    inm_u32_t * startingBitOffset,
    inm_u32_t * bitsInRun,
    inm_u32_t * bitOffset);
#endif /* _INMAGE_BITMAP_OPERATIONS_H */
