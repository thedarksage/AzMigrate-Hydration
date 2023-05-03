// This is the main project file for VC++ application project 
// generated using an Application Wizard.

#include "stdafx.h"

//#using <mscorlib.dll>
#include <tchar.h>

//using namespace System;

DIRTY_BLOCKS dirtyBlocks;
#define SECTOR_SIZE (0x200)
#define BITMAP_GRANULARITY (0x1000)
#define BITMAP_BUFFER_SIZE (0x1000)
#define BITS_PER_BITMAP_BUFFER (BITMAP_BUFFER_SIZE * 8)
#define ROUND_UP(value, quantum) ((value % quantum) ? (value + (quantum - (value % quantum))) : value)
#define ROUND_DOWN(value, quantum) ((value % quantum) ? (value - (value % quantum)) : value)

//
// Cycle the bitmap in the driver
//
DWORD CycleChangesThroughBitmap(TargetIO * target)
{
DWORD status;

    status = target->CloseShutdownNotify();
    Sleep(500);
    status = target->OpenShutdownNotify();

    return 0;
}

void PrintDirtyBlocks(DIRTY_BLOCKS * dirtyBlocks)
{
int i;
    
    printf("Nbr of changes %x\n", dirtyBlocks->cChanges);
    for(i = 0;i < dirtyBlocks->cChanges;i++) {
        printf("Change Entry %X offset %I64X size %X\n", i,
                dirtyBlocks->Changes[i].ByteOffset,
                dirtyBlocks->Changes[i].Length);
    }
}


//
// This function checks that writes of sector size get quantized into bitmap granularity size
//
DWORD VerifyQuantization(TargetIO * target)
{
char buffer[(BITMAP_GRANULARITY * 4)];
int i;
int quantizedSize;
__int64 quantizedOffset;

    ZeroMemory(buffer, sizeof(buffer));
    printf("Testing quantization...\n");

    for(i = 0;i < (BITMAP_GRANULARITY * 4); i += SECTOR_SIZE) {
        // write one run and don't force it to the bitmap, should return the same size as write
        target->Write(0, i, &buffer);
        target->GetDirtyBlocks(&dirtyBlocks);
        if ((dirtyBlocks.cChanges == 1) &&
            (dirtyBlocks.Changes[0].ByteOffset.QuadPart == 0L) &&
            (dirtyBlocks.Changes[0].Length == i)) 
        {
            printf("Quantization correct for dirtyblock entry size %x\n", i);
        } else if ((i == 0) && (dirtyBlocks.cChanges == 0))
        {
            printf("Quantization correct for dirtyblock entry size %x\n", i);
        } else
        {
            printf("ERROR:Quantization incorrect for dirtyblock entry size %x\n", i);
            PrintDirtyBlocks(&dirtyBlocks);
        }

        // now flush it through the bitmap, should get quantized
        quantizedSize = ROUND_UP(i, BITMAP_GRANULARITY);
        quantizedOffset = 0;
        target->Write(0, i, &buffer);
        CycleChangesThroughBitmap(target);
        target->GetDirtyBlocks(&dirtyBlocks);
        if ((dirtyBlocks.cChanges == 1) &&
            (dirtyBlocks.Changes[0].ByteOffset.QuadPart == quantizedOffset) &&
            (dirtyBlocks.Changes[0].Length == quantizedSize)) 
        {
            printf("Quantization correct for bitmap cycled entry size %x\n", i);
        } else if ((i == 0) && (dirtyBlocks.cChanges == 0))
        {
            printf("Quantization correct for bitmap cycled entry size %x\n", i);
        } else
        {
            printf("ERROR:Quantization incorrect for bitmap cycled entry size %x\n", i);
            PrintDirtyBlocks(&dirtyBlocks);
        }
    }
    return 0;
}

//
// This function checks that writes of sector size get quantized across bitmap granularity offsets
//
DWORD VerifyQuantizationAcrossGranularityUnit(TargetIO * target)
{
char buffer[(BITMAP_GRANULARITY * 4)];
__int64 offset;
int quantizedSize;
__int64 quantizedOffset;

    ZeroMemory(buffer, sizeof(buffer));
    printf("Testing quantization across granularity unit...\n");

    for(offset = 0;offset < ((BITMAP_GRANULARITY * 4) - (SECTOR_SIZE * 4)); offset += SECTOR_SIZE) {
        // write one run and don't force it to the bitmap, should not change in size
        target->Write(offset, (SECTOR_SIZE * 4), &buffer);
        target->GetDirtyBlocks(&dirtyBlocks);
        if ((dirtyBlocks.cChanges == 1) &&
            (dirtyBlocks.Changes[0].ByteOffset.QuadPart == offset) &&
            (dirtyBlocks.Changes[0].Length == (SECTOR_SIZE * 4))) 
        {
            printf("Quantization correct for dirtyblock entry offset %I64x\n", offset);
        } else
        {
            printf("ERROR:Quantization incorrect for dirtyblock entry offset %I64x\n", offset);
            PrintDirtyBlocks(&dirtyBlocks);
        }

        // now flush it through the bitmap, should get quantized
        quantizedSize = (int)((((SECTOR_SIZE * 4) + (offset % (__int64)BITMAP_GRANULARITY)) + (BITMAP_GRANULARITY - 1)) / BITMAP_GRANULARITY) * BITMAP_GRANULARITY;
        quantizedOffset = (offset - (offset % BITMAP_GRANULARITY));
        // write one run and don't force it to the bitmap, should not change in size
        target->Write(offset, (SECTOR_SIZE * 4), &buffer);
        CycleChangesThroughBitmap(target);
        target->GetDirtyBlocks(&dirtyBlocks);
        if ((dirtyBlocks.cChanges == 1) &&
            (dirtyBlocks.Changes[0].ByteOffset.QuadPart == quantizedOffset) &&
            (dirtyBlocks.Changes[0].Length == quantizedSize)) 
        {
            printf("Quantization correct for bitmap cycled entry offset %I64x\n", offset);
        } else
        {
            printf("ERROR:Quantization incorrect for bitmap cycled entry offset %I64x\n", offset);
            PrintDirtyBlocks(&dirtyBlocks);
        }
    }

    return 0;
}

DWORD VerifyAcrossBitmapBuffers(TargetIO * target, __int64 volumeSize, void * buffer)
{
int i;
int size;
int nbrBuffers;
unsigned long nbrBits;
__int64 offset;
int quantizedSize;
__int64 quantizedOffset;

    nbrBits = (unsigned long)ROUND_UP(volumeSize, BITMAP_GRANULARITY) / BITMAP_GRANULARITY;
    nbrBuffers = ROUND_UP(nbrBits, BITS_PER_BITMAP_BUFFER) / BITS_PER_BITMAP_BUFFER;
    printf("Testing crossing bitmap buffers...\n");

    for(i = 1; i < nbrBuffers;i++) {
        printf("Testing boundry between buffer %x and %x\n", i, i + 1);
        for(offset = (((BITMAP_BUFFER_SIZE * i) - 2) * 8); offset < (((BITMAP_BUFFER_SIZE * i) + 3) * 8);offset++) {
            for(size = 1; size < 20;size++) {
                quantizedOffset = offset * BITMAP_GRANULARITY;
                quantizedSize = size * BITMAP_GRANULARITY;
                target->Write(quantizedOffset, quantizedSize, buffer);
                CycleChangesThroughBitmap(target);
                target->GetDirtyBlocks(&dirtyBlocks);
                if ((dirtyBlocks.cChanges == 1) &&
                    (dirtyBlocks.Changes[0].ByteOffset.QuadPart == quantizedOffset) &&
                    (dirtyBlocks.Changes[0].Length == quantizedSize)) 
                {
                    printf("Correct crossing bitmap buffer boundry offset %I64x size %x\n", quantizedOffset, quantizedSize);
                } else
                {
                    printf("ERROR: Incorrect crossing bitmap buffer boundry offset %I64x size %x\n", quantizedOffset, quantizedSize);
                    PrintDirtyBlocks(&dirtyBlocks);
                }
            }
        }
    }

    return 0;
}

//
// This function checks that two write are coalesced in the bitmap correctly
//
DWORD VerifySmallRunCoalescing(TargetIO * target, void * buffer)
{
int i, j;

    printf("Testing run coalescing...\n");

    for(i = 0; i < 20;i++) {
        for(j = 0; j < 20;j++) {
            target->Write(0, i * BITMAP_GRANULARITY, buffer);
            target->Write(i * BITMAP_GRANULARITY, j * BITMAP_GRANULARITY, buffer);
            CycleChangesThroughBitmap(target);
            target->GetDirtyBlocks(&dirtyBlocks);
            if ((dirtyBlocks.cChanges == 1) &&
                (dirtyBlocks.Changes[0].ByteOffset.QuadPart == 0L) &&
                (dirtyBlocks.Changes[0].Length == (i + j) * BITMAP_GRANULARITY)) 
            {
                printf("Correct run coalescing of %x and %x\n", i,j);
            } else if (((i + j) == 0) && (dirtyBlocks.cChanges == 0))
            {
                printf("Correct run coalescing of %x and %x\n", i,j);
            } else
            {
                printf("ERROR:Incorrect run coalescing of %x and %x\n", i,j);
                PrintDirtyBlocks(&dirtyBlocks);
            }
        }
    }

    return 0;
}

//
// This function checks that two write are coalesced in the bitmap correctly
//
DWORD VerifyEndOfVolume(TargetIO * target, __int64 volumeSize, void * buffer)
{
int quantizedSize;
__int64 quantizedOffset;

    printf("Testing end of volume...\n");

    for(quantizedSize = 0; quantizedSize < (BITMAP_GRANULARITY * 16);quantizedSize += BITMAP_GRANULARITY) {
        for(quantizedOffset = ROUND_DOWN((volumeSize - (BITMAP_GRANULARITY * 16)),BITMAP_GRANULARITY); quantizedOffset < (volumeSize + (BITMAP_GRANULARITY * 16));quantizedOffset += BITMAP_GRANULARITY) {
            target->Write(quantizedOffset, quantizedSize, buffer);
            CycleChangesThroughBitmap(target);
            target->GetDirtyBlocks(&dirtyBlocks);
            if ((quantizedOffset < volumeSize) &&
                ((quantizedOffset + (__int64)quantizedSize) <= volumeSize)) 
            {
                if ((dirtyBlocks.cChanges == 1) &&
                    (dirtyBlocks.Changes[0].ByteOffset.QuadPart == quantizedOffset) &&
                    (dirtyBlocks.Changes[0].Length == quantizedSize) )
                {
                    printf("Correct end of volume handling offset %I64x size %x\n",quantizedOffset, quantizedSize );
                } else if ((quantizedSize == 0) && (dirtyBlocks.cChanges == 0))
                {
                    printf("Correct end of volume handling offset %I64x size %x\n",quantizedOffset, quantizedSize );
                } else
                {
                    printf("ERROR:Incorrect end of volume handling offset %I64x size %x\n",quantizedOffset, quantizedSize );
                    PrintDirtyBlocks(&dirtyBlocks);
                }
            } else
            {
                if (dirtyBlocks.cChanges == 0) 
                {
                    printf("Correct end of volume handling offset %I64x size %x\n",quantizedOffset, quantizedSize );
                } else
                {
                    printf("ERROR:Incorrect end of invalid write parameters offset %I64x size %x\n",quantizedOffset, quantizedSize );
                    PrintDirtyBlocks(&dirtyBlocks);
                }
            }
        }
    }

    return 0;
}

DWORD VerifyLargeRunCoalescing(TargetIO * target, __int64 volumeSize, void * buffer, long bufferSize)
{
__int64 offset;
int i;
__int64 changeSize;

    ZeroMemory(buffer, bufferSize);
    printf("Testing large run coalescing...\n");

    offset = 0;
    while (offset < volumeSize) {
        target->Write(offset, bufferSize, buffer);
        offset += bufferSize;
    }

    offset -= bufferSize;

    CycleChangesThroughBitmap(target);
    target->GetDirtyBlocks(&dirtyBlocks);

    if (dirtyBlocks.cChanges == (offset  / BITMAP_GRANULARITY / BITS_PER_BITMAP_BUFFER) + 1)
    {
        printf("Correct large run coalescing runs sized %x up to offset %I64x\n",bufferSize, offset);
    } else
    {
        changeSize = 0;
        for(i = 0;i < dirtyBlocks.cChanges;i++)
        {
            changeSize += dirtyBlocks.Changes[i].Length;
        }
        if (changeSize == offset) {
            printf("Correct large run coalescing runs sized %x up to offset %I64x\n",bufferSize, offset);
        } else
        {
            printf("ERROR: Incorrect large run coalescing runs sized %x up to offset %I64x\n",bufferSize, offset);
            PrintDirtyBlocks(&dirtyBlocks);
        }
    }

    return 0;
}

DWORD VerifyManyDisjointRuns(TargetIO * target, __int64 volumeSize)
{
char buffer[BITMAP_GRANULARITY];
__int64 offset;
int i;
int runs;
int processedRuns;
int badRuns;

    ZeroMemory(buffer, sizeof(buffer));
    printf("Testing many disjoint runs...\n");

    offset = 0;
    runs = 0;
    while ((offset + BITMAP_GRANULARITY) < volumeSize) {
        runs++;
        if ((runs % 10000) == 0)
        {
            printf("Writing run %x\n", runs);
        }
        target->Write(offset, BITMAP_GRANULARITY, buffer);
        offset += BITMAP_GRANULARITY * 2;
    }

    CycleChangesThroughBitmap(target);

    processedRuns = 0;
    badRuns = 0;
    target->GetDirtyBlocks(&dirtyBlocks);
    while (dirtyBlocks.cChanges > 0) {
        processedRuns += dirtyBlocks.cChanges;
        for (i = 0; i < dirtyBlocks.cChanges;i++) {
            if (dirtyBlocks.Changes[i].Length != BITMAP_GRANULARITY)
                badRuns++;
        }
        printf("Reading run %x\n", processedRuns);
        target->GetDirtyBlocks(&dirtyBlocks);
    }

    if ((badRuns == 0) && (processedRuns == runs)) 
    {
        printf("Correct result for many disjoint runs %x\n",runs);
    } else
    {
        printf("ERROR: Incorrect result for many disjoint runs, wrote %x read %x wrong size runs %x\n",runs, processedRuns, badRuns);
    }

    return 0;
}

DWORD VerifyManyDisjointRunsWithShutdown(TargetIO * target, __int64 volumeSize, bool verifyOnly)
{
char buffer[BITMAP_GRANULARITY];
__int64 offset;
int i;
int runs;
int processedRuns;
int badRuns;

    ZeroMemory(buffer, sizeof(buffer));
    printf("Testing many disjoint runs...\n");

    offset = 0;
    runs = 0;
    while ((offset + BITMAP_GRANULARITY) < volumeSize) {
        runs++;
        if (!verifyOnly) {
            if ((runs % 10000) == 0)
            {
                printf("Writing run %x\n", runs);
            }
            target->Write(offset, BITMAP_GRANULARITY, buffer);
        }
        offset += BITMAP_GRANULARITY * 2;
    }

    if (!verifyOnly) {
        return 0;
    }

    // CycleChangesThroughBitmap(target);
    // Sleep(10000);

    processedRuns = 0;
    badRuns = 0;
    target->GetDirtyBlocks(&dirtyBlocks);
    while (dirtyBlocks.cChanges > 0) {
        processedRuns += dirtyBlocks.cChanges;
        for (i = 0; i < dirtyBlocks.cChanges;i++) {
            if (dirtyBlocks.Changes[i].Length != BITMAP_GRANULARITY)
                badRuns++;
        }
        printf("Reading run %x\n", processedRuns);
        target->GetDirtyBlocks(&dirtyBlocks);
    }

    if ((badRuns == 0) && (processedRuns == runs)) 
    {
        printf("Correct result for many disjoint runs %x\n",runs);
    } else
    {
        printf("ERROR: Incorrect result for many disjoint runs, wrote %x read %x wrong size runs %x\n",runs, processedRuns, badRuns);
    }

    return 0;
}


DWORD RedirectDirtyBlocksToNamedPipe(TargetIO * target)
{
HANDLE hPipe;
DWORD remoteBufferSize;
DWORD xferBytes;
BOOLEAN fSuccess;

    hPipe = CreateNamedPipe("\\\\.\\pipe\\InVolFlt",
                            PIPE_ACCESS_DUPLEX,     
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,              
                            1,
                            sizeof(DIRTY_BLOCKS), 
                            sizeof(DIRTY_BLOCKS), 
                            1000*15,            
                            NULL);                   

    if (hPipe == INVALID_HANDLE_VALUE) {
        printf("ERROR:Unable to open named pipe %x\n",GetLastError());
        exit(255);
    }

    if (ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED)) {
        while (true) {
            fSuccess = ReadFile( 
                hPipe,
                &remoteBufferSize, 
                sizeof(remoteBufferSize),
                &xferBytes,
                NULL);       

            if (fSuccess && (xferBytes == sizeof(remoteBufferSize))) {
                target->GetDirtyBlocks(&dirtyBlocks);
 
                fSuccess = WriteFile( 
                    hPipe,
                    &dirtyBlocks,
                    sizeof(dirtyBlocks), 
                    &xferBytes,
                    NULL);
            }
        }
    }
    
    CloseHandle(hPipe);

    return 0;
}


// This is the entry point for this application
int _tmain(int argc, char ** argv)
{
TargetIO target;
DWORD status;
void * buffer;
__int64 volumeSize;
char * options;

    if (argc < 2) {
        printf("Usage PatternVerifier <volume (i.e. \\\\.\\j:)><ABCDEFGHIJKLMN or empty=all><remote machine Windows name>\n"
                "\tA=basic quantization\n"
                "\tB=quantization across granularity boundry\n"
                "\tC=runs across bitmap buffers\n"
                "\tD=small run coalescing\n"
                "\tE=end of volume handling\n"
                "\tF=large run coalescing\n"
                "\tG=many small disjoint runs\n"
                "\tH=many small runs write only\n"
                "\tI=many small runs verify only\n"
                "\tR=get dirty blocks from server\n"
                "\tS=act as dirty blocks server\n");
        exit(255);
    }

    if (argc < 3)
    {
        options = "ABCDEFGHIJKLMNOPQTUVWXYZ";
    } else
    {
        options = argv[2];
    }

    // create a big buffer 1 MByte
    buffer = VirtualAlloc(NULL, 1024*1024*8, MEM_COMMIT, PAGE_READWRITE);
    ZeroMemory(buffer, 1024*1024*8);

    target.SetPath(argv[1]);

    if (!target.VerifyIsRAWVolume()) {
        printf("ERROR: This utility needs a RAW volume\n");
        exit(255);
    }

    status = target.Open();
    printf("Open status:%x\n", status);
    if (status != ERROR_SUCCESS) {
        exit(255);
    }

    if ((argc == 4) && strstr(options, "R")) {
        target.OpenRemote(argv[3]);
    }

    target.GetVolumeSize(&volumeSize);
    printf("Volume size %I64X\n", volumeSize);

    if (strstr(options, "S")) {
        RedirectDirtyBlocksToNamedPipe(&target);
        exit(0);
    }

    status = target.OpenShutdownNotify();
    printf("OpenShutdownNotify status:%x\n", status);

    if (strstr(options, "I")) {
        printf("Not clearing changes\n");
    }
    else {
        target.GetDirtyBlocks(&dirtyBlocks);
        printf("Clearing Changes %i\n", dirtyBlocks.cChanges);
    }
    
    if (strstr(options, "A"))
        VerifyQuantization(&target);
    if (strstr(options, "B"))
        VerifyQuantizationAcrossGranularityUnit(&target);
    /// we know quantization fully works at this point
    if (strstr(options, "C"))
        VerifyAcrossBitmapBuffers(&target, volumeSize, buffer);
    if (strstr(options, "D"))
        VerifySmallRunCoalescing(&target, buffer);
    if (strstr(options, "E"))
        VerifyEndOfVolume(&target,volumeSize, buffer);
    if (strstr(options, "F"))
        VerifyLargeRunCoalescing(&target,volumeSize, buffer, 1024*1024*8);
    if (strstr(options, "G"))
        VerifyManyDisjointRuns(&target, volumeSize);
    if (strstr(options, "H"))
        VerifyManyDisjointRunsWithShutdown(&target, volumeSize, false);
    if (strstr(options, "I"))
        VerifyManyDisjointRunsWithShutdown(&target, volumeSize, true);


    status = target.Close();
    printf("Close status:%x\n", status);

}