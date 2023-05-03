//
// Copyright InMage Systems 2004
//

#include "global.h"

// all this code assumes bit ordering within a byte is like this:
//              7 6 5 4 3 2 1 0  bit number within byte
//             |x x x x x x x x| where UCHAR=0x01 would have bit 0=1 and UCHAR=0x80 would have bit 7=1
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
const UCHAR bitSetTable[9][8] 
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
const UCHAR bitSearchOffsetTable[256] = 
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

const UCHAR bitSearchCountTable[256] =  
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
const UCHAR lastByteMaskTable[9] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF}; 

// these are codes that are split into 4 bytes to control the bit manipulation
// they merge an entry from bitSetTable into a byte in the desired operation
#define OpValueSetBits (0xFF000000)
#define OpValueClearBits (0xFFFF00FF)
#define OpValueInvertBits (0x00FF00FF)
#define MAX_BITRUN_LEN  (0x400) //1024

RETURNCODE ProcessBitRun(UCHAR * bitBuffer,
                         ULONG32 bitsInBitmap,
                         ULONG32 bitsInRun,
                         ULONG32 bitOffset,
                         ULONG32 * nbrBytesChanged,
                         UCHAR * *firstByteChanged,
                         ULONG32 opValue)
{
TRC
RETURNCODE status;
ULONG32 bytesTouched;
ULONG32 bitBufferSize;
ULONG32 bitsInFirstByte;
ULONG32 bitOffsetInFirstByte;
UCHAR * firstByteTouched;
UCHAR ch; // a byte to process
UCHAR xor1Mask; // first xor applied to ch
UCHAR xor2Mask; // xor applied to bitSetTable entry and then xored with ch
UCHAR xor3Mask; // final xor applied to ch
UCHAR andMask;  // and applied to bitSetTable entry and then ored with ch

    status = STATUS_SUCCESS;

    // we need to keep track of which bytes we change so we can write the correct sectors to disk
    bytesTouched = 0;
    firstByteTouched = bitBuffer;

    bitBufferSize = (bitsInBitmap + 7) / 8; // round up to nbr of bytes need to contain bitmap

    if (bitsInRun == 0) {
        // done't do anything
    } else if ((bitOffset >= bitsInBitmap) ||
               (bitsInRun > bitsInBitmap) ||
               ((bitOffset + bitsInRun) > bitsInBitmap)) {     // check that we don't overflow this bitmap segment
        status = (RETURNCODE) STATUS_ARRAY_BOUNDS_EXCEEDED;
    } else {
        // set the operation values, these are somewhat like rasterop values used in a bitblt
        xor1Mask = (UCHAR)(opValue & 0xFF);
        xor2Mask = (UCHAR)((opValue >> 8) & 0xFF);
        xor3Mask = (UCHAR)((opValue >> 16) & 0xFF);
        andMask = (UCHAR)((opValue >>24) & 0xFF);

        // move the bitmap byte pointer up to the correct byte for the first opeation
        bitBuffer += (bitOffset / 8);

        // handle the first possibly partial byte
        bytesTouched = 1;
        firstByteTouched = bitBuffer;
        bitOffsetInFirstByte = ((ULONG32)bitOffset & 0x7); // one of 8 offsets, same as % 8
        bitsInFirstByte = min(min(bitsInRun, 8), (8 - bitOffsetInFirstByte));

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

RETURNCODE SetBitmapBitRun(UCHAR * bitBuffer,
                    ULONG32 bitsInBitmap,
                    ULONG32 bitsInRun,
                    ULONG32 bitOffset,
                    ULONG32 * nbrBytesChanged,
                    UCHAR * *firstByteChanged) // note this is a POINTER to UCHAR pointer
{
TRC
    return ProcessBitRun(bitBuffer, bitsInBitmap, bitsInRun, bitOffset, nbrBytesChanged, firstByteChanged, OpValueSetBits);
}

RETURNCODE ClearBitmapBitRun(UCHAR * bitBuffer,
                        ULONG32 bitsInBitmap,
                        ULONG32 bitsInRun,
                        ULONG32 bitOffset,
                        ULONG32 * nbrBytesChanged,
                        UCHAR * *firstByteChanged) // note this is a POINTER to UCHAR pointer
{
TRC
    return ProcessBitRun(bitBuffer, bitsInBitmap, bitsInRun, bitOffset, nbrBytesChanged, firstByteChanged, OpValueClearBits);
}

RETURNCODE InvertBitmapBitRun(UCHAR * bitBuffer,
                         ULONG32 bitsInBitmap,
                         ULONG32 bitsInRun,
                         ULONG32 bitOffset,
                         ULONG32 * nbrBytesChanged,
                         UCHAR * *firstByteChanged) // note this is a POINTER to UCHAR pointer
{
TRC
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
RETURNCODE GetNextBitmapBitRun(
    UCHAR * bitBuffer,
    ULONG32 totalBitsInBitmap,   // in BITS not bytes
    ULONG32 * startingBitOffset, // in and out parameter, set search start and is updated, relative to start of bitBuffer
    ULONG32 * bitsInRun, // 0 means no run found, can be up to totalBitsInBitmap
    ULONG32 * bitOffset) // output bit offset relative to bitBuffer, meaningless value if *bitsInRun == 0
{
TRC
RETURNCODE status;
ULONG32 bitsAvailableToSearch;
ULONG32 runOffset; 
ULONG32 runLength;
ULONG32 bitsDiscardedOnFirstByteSearched;
ULONG32 bitsDiscardedOnFirstByteOfRun;
ULONG32 bitsInLastByte;
UCHAR ch;
BOOLEAN BitRunBreak = FALSE;
BOOLEAN bLoopAlwaysFalse = FALSE;

    ASSERT(*startingBitOffset < totalBitsInBitmap);

    runOffset = *startingBitOffset; // the minimum offset it could be
    runLength = 0;

    if ((totalBitsInBitmap % 8) == 0)
    {
        bitsInLastByte = 8;
    } else
    {
        bitsInLastByte = (totalBitsInBitmap % 8);
    }

    status = STATUS_SUCCESS;
    bitsAvailableToSearch = totalBitsInBitmap - *startingBitOffset; // already validated this will not underflow

    // throw away full bytes from buffer that are before starting offset
    bitBuffer += (*startingBitOffset / 8);

    // get the first byte of buffer, this contains the starting offset bit
    ch = *bitBuffer++;

    // get the bits before the starting offset in the first byte shifted away
    bitsDiscardedOnFirstByteSearched = (ULONG32)(*startingBitOffset & 0x7); // this offset is already included in the starting position of runOffset
    ch = ch >> (UCHAR)bitsDiscardedOnFirstByteSearched; // throw away bits before starting point

    do {
        // is this the last byte of buffer
        if (bitsAvailableToSearch <= bitsInLastByte) {
            // on last byte of buffer, mask off any bits that are past the end of the bitmap
            ch &= lastByteMaskTable[bitsAvailableToSearch];
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
            ASSERT(((bitsAvailableToSearch - bitsInLastByte) % 8) == 0); // should be byte aligned
            ch = *bitBuffer++;
            while ((bitsAvailableToSearch > bitsInLastByte) && (ch == 0)) {
                ch = *bitBuffer++;
                bitsAvailableToSearch -= 8; // this can't underflow if the above assert passes
                runOffset += 8;
            }
        }

        // we are either at the first byte of the run or the last byte of the buffer
        if (bitsAvailableToSearch <= bitsInLastByte) {
            ASSERT(bitsAvailableToSearch == bitsInLastByte);
            // only partial byte to search, mask off trailing unused bits
            ch &= lastByteMaskTable[bitsAvailableToSearch];
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
        ASSERT(((bitsAvailableToSearch - bitsInLastByte) % 8) == 0); // should be byte aligned
        ch = *bitBuffer++;
        bitsAvailableToSearch -= 8;
        while ((bitsAvailableToSearch > bitsInLastByte) && (ch == 0xFF)){
            /* Check whether the # bits in next byte can fit in the current run length */
            if ((runLength + 8) > MAX_BITRUN_LEN){
                BitRunBreak = TRUE;
                break;
            }
            // full bytes are part of run
            ch = *bitBuffer++;
            bitsAvailableToSearch -= 8;
            runLength += 8;
        }

        if (BitRunBreak == TRUE){
            break;
        }
       
        // we know we're either at the end of the run or the end of the buffer
        if (bitsAvailableToSearch <= bitsInLastByte) {
            // on last byte of buffer, mask off any bits that are past the end of the bitmap
            ch &= lastByteMaskTable[bitsAvailableToSearch]; // handle bitmaps of non multiple of 8 size
            /* Check whether the # bits in next byte can fit in the current run length */
            if ((runLength + bitSearchCountTable[ch]) > MAX_BITRUN_LEN) {
                break;
            }
            runLength += bitSearchCountTable[ch]; // get the number of bits starting at the lsb for this run
            break;
        }

        /* Check whether the # bits in next byte can fit in the current  run length */
        if ((runLength + bitSearchCountTable[ch]) > MAX_BITRUN_LEN) {
            break;
        }

        // run must end on this byte and this is not end of buffer
        runLength += bitSearchCountTable[ch]; // get the number of bits in the run
    } while (bLoopAlwaysFalse); // loop only used to allow break. Fixed W4 Warnings. AI P3:- Code need to be rewritten to get rid of loop

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
