
#include "stdafx.h"

bool Openfile(const char *FileName, unsigned int OpenMode, unsigned int SharedMode, HANDLE &hand)
{
    
    hand = CreateFile(FileName, GENERIC_READ, SharedMode, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hand) {
        printf("error code is %x\n",GetLastError());
        return false;
    }

    return true;
}

void *ALLOC_MEMORY(int size)
{
	return malloc( size);
}

void FREE_MEMORY(void *Buffer)
{
    free(Buffer);
}