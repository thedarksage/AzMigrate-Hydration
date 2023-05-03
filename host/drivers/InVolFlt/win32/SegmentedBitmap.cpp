// this class handles bitmaps that may not be memory resident
//
// Copyright InMage Systems 2004
//

#include <global.h>

NTSTATUS SegmentedBitmap::ProcessBitRun(ULONG32 bitsInRun, ULONG64 bitOffset, enum tagBitmapOperation operation )
{
TRC
NTSTATUS status;
UCHAR * bitBuffer;
ULONG32 bitBufferByteSize;
ULONG32 adjustedBitsInRun;
ULONG64 adjustedBufferSize;
ULONG32 nbrBytesChanged;
UCHAR * firstByteChanged = NULL;
ULONG64 byteOffset;
ULONG32 bitsToProcess;

    // this is passive level synchronous code

    nbrBytesChanged = 0;

    // check the bounds
    bitsToProcess = bitsInRun;
    if ((bitOffset + bitsInRun) > bitsInBitmap) {
        status = STATUS_ARRAY_BOUNDS_EXCEEDED;
    } else {
        status = STATUS_SUCCESS; // default of zero length run
        while (bitsToProcess > 0) {
            byteOffset = bitOffset / 8;
            status = segmentMapper->ReadAndLock(byteOffset, &bitBuffer, &bitBufferByteSize);
            if (NT_SUCCESS(status)) {
                // figure out if this run crosses segment boundries or the bitmap end
                adjustedBufferSize = min(bitBufferByteSize * 8, (bitsInBitmap - (byteOffset * 8)));
                // prevent runs that span buffers, hand that in this function
                adjustedBitsInRun = (ULONG32)min(bitsToProcess, (adjustedBufferSize - (bitOffset % 8)));
                
                switch (operation) {
                    case setBits:
                        status = SetBitmapBitRun(bitBuffer, // should point at byte with first bit
                                                (ULONG32)adjustedBufferSize, // how many bits are valid
                                                adjustedBitsInRun, // nbr of bits to change
                                                (ULONG32)(bitOffset % 8),
                                                &nbrBytesChanged,
                                                &firstByteChanged);
                        break;
                    case clearBits:
                        status = ClearBitmapBitRun(bitBuffer, // should point at byte with first bit
                                                (ULONG32)adjustedBufferSize, // how many bits are valid
                                                adjustedBitsInRun, // nbr of bits to change
                                                (ULONG32)(bitOffset % 8),
                                                &nbrBytesChanged,
                                                &firstByteChanged);
                        break;
                    case invertBits:
                        status = InvertBitmapBitRun(bitBuffer, // should point at byte with first bit
                                                (ULONG32)adjustedBufferSize, // how many bits are valid
                                                adjustedBitsInRun, // nbr of bits to change
                                                (ULONG32)(bitOffset % 8),
                                                &nbrBytesChanged,
                                                &firstByteChanged);
                        break;
                    default:
                        ASSERTMSG("Invalid operation code passed to SegmentedBitmap::ProcessBitRun", 0);
                        break;
                }

                if (nbrBytesChanged > 0) {
                    segmentMapper->UnlockAndMarkDirty(byteOffset + (firstByteChanged - bitBuffer), nbrBytesChanged);
                } else {
                    segmentMapper->Unlock(byteOffset);
                }

                ASSERT(adjustedBitsInRun > 0);
                bitsToProcess -= adjustedBitsInRun;
                bitOffset += adjustedBitsInRun;
            } else {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
                    ("SegmentedBitmap::ProcessBitRun ReadAndLock returned status = %#x\n", status));
                ASSERT(0);
                break;
            }
        }
    }

    return status;
}

NTSTATUS SegmentedBitmap::SetBitRun(ULONG32 bitsInRun, ULONG64 bitOffset)
{
TRC
NTSTATUS status;
    // this is passive level synchronous code

    status = SegmentedBitmap::ProcessBitRun(bitsInRun, bitOffset, setBits);
    return status;
}

NTSTATUS SegmentedBitmap::ClearBitRun(ULONG32 bitsInRun, ULONG64 bitOffset)
{
TRC
NTSTATUS status;
    // this is passive level synchronous code

    status = SegmentedBitmap::ProcessBitRun(bitsInRun, bitOffset, clearBits);
    return status;
}

NTSTATUS SegmentedBitmap::InvertBitRun(ULONG32 bitsInRun, ULONG64 bitOffset)
{
TRC
NTSTATUS status;
    // this is passive level synchronous code

    status = SegmentedBitmap::ProcessBitRun(bitsInRun, bitOffset, invertBits);
    return status;
}

NTSTATUS SegmentedBitmap::ClearAllBits()
{
TRC
NTSTATUS status;
    // this is passive level synchronous code

    // bugbug only handles bitmaps up to 4GBits
    status = SegmentedBitmap::ProcessBitRun((ULONG32)bitsInBitmap, 0, clearBits);
    return status;
}

NTSTATUS SegmentedBitmap::GetFirstBitRun(ULONG32 * bitsInRun, ULONG64 *bitOffset)
{
TRC
NTSTATUS status;

    nextSearchOffset = 0;
    status = GetNextBitRun(bitsInRun, bitOffset);

    return status;
}

NTSTATUS SegmentedBitmap::GetNextBitRun(ULONG32 * bitsInRun, ULONG64 *bitOffset)
{
TRC
NTSTATUS status;
UCHAR * bitBuffer;
ULONG32 bitBufferByteSize;
ULONG32 adjustedBufferSize;
ULONG64 byteOffset;
ULONG32 searchBitOffset;
ULONG32 runOffset;
ULONG32 runLength;

    // this is passive level synchronous code currently
    *bitsInRun = 0;
    *bitOffset = 0;
    runLength = 0;
    runOffset = 0;
    status = STATUS_SUCCESS;

    while (nextSearchOffset < bitsInBitmap) {
        byteOffset = nextSearchOffset / 8;
        status = segmentMapper->ReadAndLock(byteOffset, &bitBuffer, &bitBufferByteSize);
        if (NT_SUCCESS(status)) {
            searchBitOffset = (ULONG32)(nextSearchOffset % 8);
            adjustedBufferSize = (ULONG32)min((bitBufferByteSize * 8),
                                              (bitsInBitmap - (byteOffset * 8)));

            status = GetNextBitmapBitRun(bitBuffer, 
                            adjustedBufferSize, 
                            &searchBitOffset, // in and out parameter, set search start and is updated
                            &runLength,
                            &runOffset);
            /* status = */ segmentMapper->Unlock(byteOffset);
            if (!NT_SUCCESS(status)) {
                break;
            }
            // searchBitOffset and runOffset will return relative to start of bitBuffer not old nextSearchOffset
            nextSearchOffset = (byteOffset * 8) + searchBitOffset;
            if (runLength > 0) {
                *bitOffset = (byteOffset * 8) + runOffset;
                break;
            } 
        } else {
            // segment mapper could not map buffer
            break;
        }
    }

    if (NT_SUCCESS(status)) {
        if (runLength > 0) {
            *bitsInRun = runLength;
            status = STATUS_SUCCESS;
        } else {
            *bitsInRun = 0;
            *bitOffset = nextSearchOffset;
            status = STATUS_ARRAY_BOUNDS_EXCEEDED;
        }
    }

    return status;
}

ULONG64
SegmentedBitmap::GetNumberOfBitsSet()
{
TRC
    NTSTATUS status;
    UCHAR * bitBuffer;
    ULONG32 bitBufferByteSize;
    ULONG64 adjustedBufferSize, FlooredBufferSize;
    ULONG32 nbrBytesChanged;
    UCHAR * firstByteChanged;
    ULONG64 byteOffset, bitOffset;
    ULONG64 bitsToProcess, bitsSet = 0;

    // this is passive level synchronous code

    nbrBytesChanged = 0;

    // check the bounds
    bitsToProcess = bitsInBitmap;
    bitOffset = 0;

    while (bitsToProcess > 0) {
        byteOffset = bitOffset / 8;
        status = segmentMapper->ReadAndLock(byteOffset, &bitBuffer, &bitBufferByteSize);
        if (NT_SUCCESS(status)) {
            RTL_BITMAP  RTLbitmap;

            // figure out if this run crosses segment boundries or the bitmap end
            adjustedBufferSize = min(bitBufferByteSize * 8, (bitsInBitmap - (byteOffset * 8)));
            FlooredBufferSize = ((adjustedBufferSize + sizeof(ULONG) - 1) / sizeof(ULONG)) * sizeof(ULONG);

            RtlInitializeBitMap(&RTLbitmap, (PULONG)bitBuffer, (ULONG)FlooredBufferSize);
            bitsSet += RtlNumberOfSetBits(&RTLbitmap);
            if (FlooredBufferSize < adjustedBufferSize) {
                // Calculate the bits set in remaing buffer;
                PUCHAR buffer = bitBuffer + ((ULONG)FlooredBufferSize / 8);
                ULONG   ulBitsToTest = (ULONG)(adjustedBufferSize - FlooredBufferSize);
                
                ULONG   ulBitMask = 0x80;
                for (ULONG i = 0; i < ulBitsToTest; i++) {
                    if (*buffer & ulBitMask) {
                        bitsSet++;
                    }

                    if (0x01 == ulBitMask) {
                        buffer++;
                        ulBitMask = 0x80;
                    } else {
                        ulBitMask = ulBitMask >> 1;
                    }
                }
            }

            bitsToProcess -= adjustedBufferSize;
            bitOffset += adjustedBufferSize;
            segmentMapper->Unlock(byteOffset);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
                ("SegmentedBitmap::ProcessBitRun ReadAndLock returned status = %#x\n", status));
            ASSERT(0);
            break;
        }
    }

    return bitsSet;
}

