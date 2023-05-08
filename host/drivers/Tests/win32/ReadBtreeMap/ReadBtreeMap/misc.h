#ifndef _MICS_FUNC
#define _MICS_FUNC



bool Openfile(const char *FileName, unsigned int OpenMode, unsigned int SharedMode, HANDLE &);

void *ALLOC_MEMORY(int size);
void FREE_MEMORY(void *Buffer);
#endif