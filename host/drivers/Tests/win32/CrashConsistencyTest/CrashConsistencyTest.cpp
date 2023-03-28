// This is the main project file for VC++ application project 
// generated using an Application Wizard.

#include "stdafx.h"

//#using <mscorlib.dll>
#include <tchar.h>
#include <windows.h>
#include <stdio.h>

#define FILE_SIZE (0x1000 * 256 * 10)
//using namespace System;

typedef struct {
    int count;
    char fill[0x1000 - sizeof(int)];
} tBlock;

tBlock headerBuffer;
tBlock dataBuffer;


void GenerateTransactionIDArray(char * pathname)
{
HANDLE fileHandle;
int i;
DWORD written;

    fileHandle = CreateFile(pathname,
                            GENERIC_ALL,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            CREATE_ALWAYS,
                            FILE_FLAG_NO_BUFFERING,
                            NULL);

    if (fileHandle == INVALID_HANDLE_VALUE) {
        printf("Unable to open file %s\n", pathname);
        exit(255);
    }

    ZeroMemory(&headerBuffer, sizeof(headerBuffer));
    ZeroMemory(&dataBuffer, sizeof(dataBuffer));
#if 0
    // zero the file
    for(i = 0;i < FILE_SIZE;i += 0x1000) {
        WriteFile(fileHandle, &headerBuffer, 0x1000, &written, NULL);
    }

    // wait for dirty blocks from zero fill to be processed
    printf("Pausing for 2 minutes...\n");
    Sleep(1000*60);
#endif

    // alternate writing data then header
    headerBuffer.count = 0;
    for(i = 0x1000;i < FILE_SIZE;i += 0x1000) {
        Sleep(100);
        headerBuffer.count++;
        dataBuffer.count = headerBuffer.count;
        SetFilePointer(fileHandle, i, NULL, FILE_BEGIN);
        WriteFile(fileHandle, &dataBuffer, 0x1000, &written, NULL);
        FlushFileBuffers(fileHandle);
        SetFilePointer(fileHandle, 0, NULL, FILE_BEGIN);
        WriteFile(fileHandle, &headerBuffer, 0x1000, &written, NULL);
        FlushFileBuffers(fileHandle);
    }

    CloseHandle(fileHandle);
}

void VerifyTransactionIDArray(char * pathname)
{
HANDLE fileHandle;
int i, j;
DWORD read;
DWORD size;
DWORD lastValid;

    fileHandle = CreateFile(pathname,
                            GENERIC_ALL,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_NO_BUFFERING,
                            NULL);

    if (fileHandle == INVALID_HANDLE_VALUE) {
        printf("Unable to open file %s\n", pathname);
        exit(255);
    }

    ZeroMemory(&headerBuffer, sizeof(headerBuffer));
    ZeroMemory(&dataBuffer, sizeof(headerBuffer));

    SetFilePointer(fileHandle, 0, NULL, FILE_BEGIN);
    ReadFile(fileHandle, &headerBuffer, 0x1000, &read, NULL);

    // scan through the array blocks to find the last one written
    j = 0;
    lastValid = 0;
    size = GetFileSize(fileHandle, NULL);
    for(i = 0x1000;i < size;i += 0x1000) {
        read = 0;
        SetFilePointer(fileHandle, i, NULL, FILE_BEGIN);
        ReadFile(fileHandle, &dataBuffer, 0x1000, &read, NULL);
        if ((dataBuffer.count != (i / 0x1000)) || (read != 0x1000)) {
            if ((read == 0x1000) && ((dataBuffer.count != (i / 0x1000)))) {
                printf("Data buffer %i is corrupted, this suggests file size/allocation metadata is damaged\n", (i / 0x1000));
            }
            break;
        }
        lastValid++;
    }
    printf("Last record from header %i, last valid data record written %i\n",headerBuffer.count, (i / 0x1000) - 1);
    if ((dataBuffer.count == headerBuffer.count) ||
        (dataBuffer.count == (headerBuffer.count + 1)))
        printf("Header and data are as expected (header may be one less than data)\n");
    if ((lastValid * 0x1000) != (size - 0x1000))
        printf("File size metadata is incorrect, last data record should be %i\n", (size / 0x1000)- 1);

    CloseHandle(fileHandle);
}


// This is the entry point for this application
int _tmain(int argc, char **argv)
{
    if (argc == 1) {
        printf("Usage CrashConsistencyTest <action>\n"
            "\twhere action is:\n"
            "\tt = write transaction test file\n"
            "\tT = verify transaction test file\n");
         exit(255);
    }

    if (argc == 2) {
        switch (argv[1][0]) {
            case 't':
                GenerateTransactionIDArray("TransactionTest.tmp");
                break;
            case 'T':
                VerifyTransactionIDArray("TransactionTest.tmp");
                break;
            default:
                printf("Usage CrashConsistencyTest <action>\n"
                    "\twhere action is:\n"
                    "\tt = write transaction test file\n"
                    "\tT = verify transaction test file\n");
                exit(255);
        }
    }

    return 0;
}