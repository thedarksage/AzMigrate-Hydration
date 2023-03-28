

RETURNCODE SetBitmapBitRun(UCHAR * bitBuffer,
                    ULONG32 bitsInBitmap,
                    ULONG32 bitsInRun,
                    ULONG32 bitOffset,
                    ULONG32 * nbrBytesChanged,
                    UCHAR * *firstByteChanged); // note this is a POINTER to UCHAR pointer

RETURNCODE ClearBitmapBitRun(UCHAR * bitBuffer,
                        ULONG32 bitsInBitmap,
                        ULONG32 bitsInRun,
                        ULONG32 bitOffset,
                        ULONG32 * nbrBytesChanged,
                        UCHAR * *firstByteChanged); // note this is a POINTER to UCHAR pointer

RETURNCODE InvertBitmapBitRun(UCHAR * bitBuffer,
                         ULONG32 bitsInBitmap,
                         ULONG32 bitsInRun,
                         ULONG32 bitOffset,
                         ULONG32 * nbrBytesChanged,
                         UCHAR * *firstByteChanged); // note this is a POINTER to UCHAR pointer

RETURNCODE GetNextBitmapBitRun(UCHAR * bitBuffer,
                         ULONG32 totalBitsInBitmap,   // in BITS not bytes
                         ULONG32 * startingBitOffset, // in and out parameter, set search start and is updated
                         ULONG32 * bitsInRun,
                         ULONG32 * bitOffset);
