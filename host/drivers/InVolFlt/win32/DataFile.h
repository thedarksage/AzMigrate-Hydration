#ifndef __INVOLFLT_DATA_FILE_H__
#define __INVOLFLT_DATA_FILE_H__

#include "FileStream.h"

class CDataFile
{
public:
    CDataFile();
    ~CDataFile();

    NTSTATUS Open(PUNICODE_STRING FileName, ULONGLONG allocationSize, ULONG ulBufferSize);
    NTSTATUS Write(PUCHAR Buffer, ULONG ulMaxLength, ULONG ulLength);
    NTSTATUS Delete();
    NTSTATUS Close();
    ULONGLONG GetFileSize() { return m_ullFileOffset; }

    static NTSTATUS DeleteFile(PUNICODE_STRING FileName);
    static NTSTATUS DeleteDataFilesInDirectory(PUNICODE_STRING Dir);
private:
    FileStream  m_File;
    ULONGLONG   m_ullFileOffset;
    bool        m_bFileClosed;
    bool        m_bFileOpenSucceeded;
	// Last write to the file, would be a write not a multiple of buffer size.
	// As we can not do non-sector size writes, we write more than requested.
	// after this write no more writes should be done to the file.
	bool		m_bLastWriteOccured;
};

#endif // __INVOLFLT_DATA_FILE_H__
