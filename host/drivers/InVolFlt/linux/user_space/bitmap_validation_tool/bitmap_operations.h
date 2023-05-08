#ifndef _INMAGE_BITMAP_OPERATIONS_H
#define _INMAGE_BITMAP_OPERATIONS_H

unsigned int find_number_of_bits_set(unsigned char *bit_buffer,
             unsigned int buffer_size_in_bits);

int SetBitmapBitRun(unsigned char * bitBuffer,
                    unsigned int bitsInBitmap,
                    unsigned int bitsInRun,
                    unsigned int bitOffset,
                    unsigned int * nbrBytesChanged,
                    unsigned char * *firstByteChanged);
int ClearBitmapBitRun(unsigned char * bitBuffer,
                        unsigned int bitsInBitmap,
                        unsigned int bitsInRun,
                        unsigned int bitOffset,
                        unsigned int * nbrBytesChanged,
                        unsigned char * *firstByteChanged);

int InvertBitmapBitRun(unsigned char * bitBuffer,
                         unsigned int bitsInBitmap,
                         unsigned int bitsInRun,
                         unsigned int bitOffset,
                         unsigned int * nbrBytesChanged,
                         unsigned char * *firstByteChanged);

int GetNextBitmapBitRun(
    unsigned char * bitBuffer,
    unsigned int totalBitsInBitmap,
    unsigned int * startingBitOffset,
    unsigned int * bitsInRun,
    unsigned int * bitOffset);
#endif /* _INMAGE_BITMAP_OPERATIONS_H */
