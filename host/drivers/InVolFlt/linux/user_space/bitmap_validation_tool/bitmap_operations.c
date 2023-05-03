//
// Copyright InMage Systems 2005
//
#include <stdio.h>
#define min(a, b)	((a) > (b) ? (a) : (b))

#include "bitmap_operations.h"

// all this code assumes bit ordering within a byte is like this:
//              7 6 5 4 3 2 1 0  bit number within byte
//             |x x x x x x x x| where unsigned char=0x01 would have bit 0=1 and unsigned char=0x80 would have bit 7=1
//
// This means bit 17(decimal) in a char[3] array would be here:
//          23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
//         | 0  0  0  0  0  0  1  0| 0  0  0  0  0  0  0  0| 0  0  0  0  0  0  0  0|
//         |          ch[2]        |          ch[1]        |          ch[0]        | 
//
// I believe nearly all processors do it this way, even if the WORD endian is big-endian, char arrays
// are not affected
// 


// [numberOfSetBits][bitOffsetInByte]
// this table is for setting a nbr of bits in a byte at a specific bit offset
const unsigned char bitSetTable[9][8] 
//              bit offset    0     1     2     3     4     5     6     7
                        = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0 bits
                           0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, // 1 bits
                           0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00, // 2 bits, note that 0x00 means invalid
                           0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0x00, 0x00, // 3 bits
                           0x0F, 0x1E, 0x3C, 0x78, 0xF0, 0x00, 0x00, 0x00, // 4 bits
                           0x1F, 0x3E, 0x7C, 0xF8, 0x00, 0x00, 0x00, 0x00, // 5 bits
                           0x3F, 0x7E, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, // 6 bits
                           0x7F, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 7 bits
                           0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // 8 bits


// this table is used for searching for bit runs, it tells the bit offset of the first set bit
const unsigned char bitSearchOffsetTable[256] = 
                                //  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                  { 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x1x */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x2x */ 0x05, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x3x */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x4x */ 0x06, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x5x */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x6x */ 0x05, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,                              
                         /* 0x7x */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x8x */ 0x07, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0x9x */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0xAx */ 0x05, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0xBx */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0xCx */ 0x06, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0xDx */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
                         /* 0xEx */ 0x05, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,                              
                         /* 0xFx */ 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00};


// this table is used for searching for bit runs, it tells the number of bits that are contiguous from the lsb, use with shifting

const unsigned char bitSearchCountTable[256] =  
                                //  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 
                                  { 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0x00
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x05, // 0x10
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0x20
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x06, // 0x30
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0x40
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x05, // 0x50
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0x60
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x07, // 0x70
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0x80
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x05, // 0x90
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0xa0
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x06, // 0xb0
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0xc0
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x05, // 0xd0
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, // 0xe0
                                    0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x08};// 0xf0


// this table is used to mask the last byte in a buffer so we don't test bits we're not supposed to
// index will be number of bits remaining as valid
const unsigned char lastByteMaskTable[9] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF}; 

// these are codes that are split into 4 bytes to control the bit manipulation
// they merge an entry from bitSetTable into a byte in the desired operation
#define OpValueSetBits (0xFF000000)
#define OpValueClearBits (0xFFFF00FF)
#define OpValueInvertBits (0x00FF00FF)


/* Number of set bits for each value of a nibble; used for counting */
const unsigned char nibble_bit_count[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};



int ProcessBitRun(unsigned char * bitBuffer,
                         unsigned int bitsInBitmap,
                         unsigned int bitsInRun,
                         unsigned int bitOffset,
                         unsigned int * nbrBytesChanged,
                         unsigned char * *firstByteChanged,
                         unsigned int opValue)
{

int status;
unsigned int bytesTouched;
unsigned int bitBufferSize;
unsigned int bitsInFirstByte;
unsigned int bitOffsetInFirstByte;
unsigned char * firstByteTouched;
unsigned char ch; // a byte to process
unsigned char xor1Mask; // first xor applied to ch
unsigned char xor2Mask; // xor applied to bitSetTable entry and then xored with ch
unsigned char xor3Mask; // final xor applied to ch
unsigned char andMask;  // and applied to bitSetTable entry and then ored with ch

    status = 0; //success

    // we need to keep track of which bytes we change so we can write the correct sectors to disk
    bytesTouched = 0;
    firstByteTouched = bitBuffer;

    bitBufferSize = (bitsInBitmap + 7) / 8; // round up to nbr of bytes need to contain bitmap

    if (bitsInRun == 0) {
        // done't do anything
    } else if ((bitOffset >= bitsInBitmap) ||
               (bitsInRun > bitsInBitmap) ||
               ((bitOffset + bitsInRun) > bitsInBitmap)) {     // check that we don't overflow this bitmap segment
        status = EOF;
    } else {
        // set the operation values, these are somewhat like rasterop values used in a bitblt
        xor1Mask = (unsigned char)(opValue & 0xFF);
        xor2Mask = (unsigned char)((opValue >> 8) & 0xFF);
        xor3Mask = (unsigned char)((opValue >> 16) & 0xFF);
        andMask = (unsigned char)((opValue >>24) & 0xFF);

        // move the bitmap byte pointer up to the correct byte for the first opeation
        bitBuffer += (bitOffset / 8);

        // handle the first possibly partial byte
        bytesTouched = 1;
        firstByteTouched = bitBuffer;
        bitOffsetInFirstByte = ((unsigned int)bitOffset & 0x7); // one of 8 offsets, same as % 8
        bitsInFirstByte = min((min(bitsInRun, (unsigned int)8)), (8 - bitOffsetInFirstByte));

        // this code allows doing set or clear or invert of bits
        ch = *bitBuffer;
        ch ^= xor1Mask;
        ch ^= ( bitSetTable[bitsInFirstByte][bitOffsetInFirstByte]) ^ xor2Mask;
        ch |= ( bitSetTable[bitsInFirstByte][bitOffsetInFirstByte]) & andMask;
        ch ^= xor3Mask;
        *bitBuffer = ch; // the byte is now transformed
        bitsInRun -= bitsInFirstByte;
        bitBuffer++;

        // handle the middle bytes, we have already checked bitmap bounds, so don't need to here
        while (bitsInRun >= 8) {
            // this code allows doing set or clear or invert of bits
            ch = *bitBuffer;
            ch ^= xor1Mask;
            ch ^= 0xFF ^ xor2Mask;
            ch |= 0xFF & andMask;
            ch ^= xor3Mask;
            *bitBuffer = ch; // the byte is now transformed
            bitsInRun -= 8;
            bitBuffer++;
            bytesTouched++;
        }

        // process possible last byte (possibly less than 8 bits)
        if (bitsInRun > 0) {
            // this code allows doing set or clear or invert of bits
            ch = *bitBuffer;
            ch ^= xor1Mask;
            ch ^= (bitSetTable[bitsInRun][0]) ^ xor2Mask;
            ch |= (bitSetTable[bitsInRun][0]) & andMask;
            ch ^= xor3Mask;
            *bitBuffer = ch; // the byte is now transformed
            bytesTouched++;
            bitsInRun = 0;
        }
    }

    if (NULL != nbrBytesChanged) { // this parameter is optional
        *nbrBytesChanged = bytesTouched; 
    }

    if (NULL != firstByteChanged) {  // this parameter is optional
        *firstByteChanged = firstByteTouched;
    }

    return status;
}

int SetBitmapBitRun(unsigned char * bitBuffer,
                    unsigned int bitsInBitmap,
                    unsigned int bitsInRun,
                    unsigned int bitOffset,
                    unsigned int * nbrBytesChanged,
                    unsigned char * *firstByteChanged) // note this is a POINTER to unsigned char pointer
{
    return ProcessBitRun(bitBuffer, bitsInBitmap, bitsInRun, bitOffset, nbrBytesChanged, firstByteChanged, OpValueSetBits);
}

int ClearBitmapBitRun(unsigned char * bitBuffer,
                        unsigned int bitsInBitmap,
                        unsigned int bitsInRun,
                        unsigned int bitOffset,
                        unsigned int * nbrBytesChanged,
                        unsigned char * *firstByteChanged) // note this is a POINTER to unsigned char pointer
{
    return ProcessBitRun(bitBuffer, bitsInBitmap, bitsInRun, bitOffset, nbrBytesChanged, firstByteChanged, OpValueClearBits);
}

int InvertBitmapBitRun(unsigned char * bitBuffer,
                         unsigned int bitsInBitmap,
                         unsigned int bitsInRun,
                         unsigned int bitOffset,
                         unsigned int * nbrBytesChanged,
                         unsigned char * *firstByteChanged) // note this is a POINTER to unsigned char pointer
{
    return ProcessBitRun(bitBuffer, bitsInBitmap, bitsInRun, bitOffset, nbrBytesChanged, firstByteChanged, OpValueInvertBits);
}

// This function is the compute intensive code for turning bitmaps back into groups of bits
// It has a bunch of edge conditions:
//      1) there could be no run of bits from the starting offset to the end of the bitmap
//      2) the starting search offset can be on any bit offset of a byte
//      3) the end of the bitmap can be at any bit offset in a byte, not just ends of bytes
//      4) the run of bits can extend to any offset of the last byte in the buffer
//      5) the starting byte could also be the ending byte in the bitmap (the run after #4)
//      6) a run of bits can extend to the end of the bitmap (bounds terminate the run, not clear bit)
//      7) the starting search offset may not be be the start of the bit run
//      8) the run may start and end on adjacent bytes
//      9) the starting offset is past the end of the bitmap
//     10) the size of the bitmap could be less than 8 bits
int GetNextBitmapBitRun(
    unsigned char * bitBuffer,
    unsigned int totalBitsInBitmap,   // in BITS not bytes
    unsigned int * startingBitOffset, // in and out parameter, set search start and is updated, relative to start of bitBuffer
    unsigned int * bitsInRun, // 0 means no run found, can be up to totalBitsInBitmap
    unsigned int * bitOffset) // output bit offset relative to bitBuffer, meaningless value if *bitsInRun == 0
{
int status;
unsigned int bitsAvailableToSearch;
unsigned int runOffset; 
unsigned int runLength;
unsigned int bitsDiscardedOnFirstByteSearched;
unsigned int bitsDiscardedOnFirstByteOfRun;
unsigned int bitsInLastByte;
unsigned char ch;

    //ASSERT(*startingBitOffset < totalBitsInBitmap);

    runOffset = *startingBitOffset; // the minimum offset it could be
    runLength = 0;

    if ((totalBitsInBitmap % 8) == 0)
    {
        bitsInLastByte = 8;
    } else
    {
        bitsInLastByte = (totalBitsInBitmap % 8);
    }

    status = 0;
    bitsAvailableToSearch = totalBitsInBitmap - *startingBitOffset; // already validated this will not underflow

    // throw away full bytes from buffer that are before starting offset
    bitBuffer += (*startingBitOffset / 8);

    // get the first byte of buffer, this contains the starting offset bit
    ch = *bitBuffer++;

    // get the bits before the starting offset in the first byte shifted away
    bitsDiscardedOnFirstByteSearched = (unsigned int)(*startingBitOffset & 0x7); // this offset is already included in the starting position of runOffset
    ch = ch >> bitsDiscardedOnFirstByteSearched; // throw away bits before starting point

    // check for the first byte also being the last byte
    if (bitsAvailableToSearch < bitsInLastByte) {
        // only partial byte to search, mask off trailing unused bits
        ch &= lastByteMaskTable[bitsInLastByte];
    }

    do {
        // is this the last byte of buffer
        if (bitsAvailableToSearch <= bitsInLastByte) {
            if (ch == 0) {
                // no runs found
                break;
            } else {
                // found a run in last byte
                bitsDiscardedOnFirstByteOfRun = bitSearchOffsetTable[ch]; // get the offset of the first bit
                ch >>= bitsDiscardedOnFirstByteOfRun; // align the first set bit to lsb (little-endian)
                runOffset += bitsDiscardedOnFirstByteOfRun; 
                runLength = bitSearchCountTable[ch]; // get the number of bits in the run
                break;
            }
        } 
        
        // this is not the last byte and we need to find the first byte of run
        if (ch == 0) {
            // get aligned 
            bitsAvailableToSearch -= (8 - bitsDiscardedOnFirstByteSearched); // get aligned to byte boundry
            runOffset += (8 - bitsDiscardedOnFirstByteSearched);

            // scan for start of a run
            //ASSERT(((bitsAvailableToSearch - bitsInLastByte) % 8) == 0); // should be byte aligned
            ch = *bitBuffer++;
            //  BUGBUG ??? should the bitsAvailableToSearch be decremented from the above read???
            while ((bitsAvailableToSearch > bitsInLastByte) && (ch == 0)) {
                ch = *bitBuffer++;
                bitsAvailableToSearch -= 8; // this can't underflow if the above //ASSERT passes
                runOffset += 8;
            }
        }

        // we are either at the first byte of the run or the last byte of the buffer
        if (bitsAvailableToSearch <= bitsInLastByte) {
            // only partial byte to search, mask off trailing unused bits
            ch &= lastByteMaskTable[bitsInLastByte];
            if (ch) {
                // on last byte, run found
                bitsDiscardedOnFirstByteOfRun = bitSearchOffsetTable[ch]; // get the offset of the first bit
                ch >>= bitsDiscardedOnFirstByteOfRun; // align the first set bit to lsb (little-endian)
                runOffset += bitsDiscardedOnFirstByteOfRun; 
                runLength = bitSearchCountTable[ch]; // get the number of bits in the run
                break;
            } else {
                // on last byte of buffer, no run found
                break;
            }
        }

        // we must be at the start of a run, and not the end of the buffer
        bitsDiscardedOnFirstByteOfRun = bitSearchOffsetTable[ch]; // get the offset of the first bit
        ch >>= bitsDiscardedOnFirstByteOfRun; // align the first set bit to lsb (little-endian)
        runOffset += bitsDiscardedOnFirstByteOfRun; // this will be the final runOffset position
        runLength = bitSearchCountTable[ch]; // get the number of bits of the run in this byte
        if ((bitsDiscardedOnFirstByteOfRun + runLength) < 8) {
            // we must have found a run that doesn't continue to the next byte
            break;
        } 
        
        // the run might continue or not
        //ASSERT(((bitsAvailableToSearch - bitsInLastByte) % 8) == 0); // should be byte aligned
        ch = *bitBuffer++;
        bitsAvailableToSearch -= 8;
        while ((bitsAvailableToSearch > bitsInLastByte) && (ch == 0xFF)) {
            // full bytes are part of run
            ch = *bitBuffer++;
            bitsAvailableToSearch -= 8;
            runLength += 8;
        }

        // we know we're either at the end of the run or the end of the buffer
        if (bitsAvailableToSearch <= bitsInLastByte) {
            // on last byte of buffer, mask off any bits that are past the end of the bitmap
            ch &= lastByteMaskTable[bitsAvailableToSearch]; // handle bitmaps of non multiple of 8 size
            runLength += bitSearchCountTable[ch]; // get the number of bits starting at the lsb for this run
            break;
        }
        
        // run must end on this byte and this is not end of buffer
        runLength += bitSearchCountTable[ch]; // get the number of bits in the run
    } while (0); // loop only used to allow break

    if (runLength == 0) {
        // no bits past startingOffset
        *startingBitOffset = totalBitsInBitmap;
    } else {
        *startingBitOffset = runOffset + runLength; // update for next run search
    }

    *bitsInRun = runLength;
    *bitOffset = runOffset;

    return status;
}

unsigned int find_number_of_bits_set(unsigned char *bit_buffer, unsigned int buffer_size_in_bits)
{
	unsigned char *buffer;
	unsigned int byte_count, remainder_bits;
	unsigned int total_bits;
	unsigned char remainder_byte;

	if (!bit_buffer || buffer_size_in_bits == 0)
		return 0;

	buffer = bit_buffer;
	byte_count = buffer_size_in_bits/8;
	total_bits = 0;
	
	remainder_bits = buffer_size_in_bits & 0x7;

	while(byte_count--)
	{
		total_bits += nibble_bit_count[*buffer >> 4]; // find bits correspond to top nibble
		total_bits += nibble_bit_count[*buffer & 0xf]; //find bits correspond to lower nibble of byte
		buffer++;
	}

	
	remainder_byte = *buffer & lastByteMaskTable[remainder_bits];
	total_bits += nibble_bit_count[remainder_byte >> 4];
	total_bits += nibble_bit_count[remainder_byte & 0xf];

	return total_bits;
}

