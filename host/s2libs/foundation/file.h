/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : file.h
*
* Description: 
*/

#ifndef FILE__H
#define FILE__H

#ifdef SV_WINDOWS
#include <windows.h>
#endif


#include "synchronize.h"
#include "genericfile.h"
#include "fileclusterinformer.h"

typedef unsigned long int FILE_ATTRIBUTE;

class File: public GenericFile
{
public:
    virtual ~File();
    File(const File&);
    File(const std::string&, bool check_for_existence = true);
    File(const char*, bool check_for_existence = true);


    const std::string& GetPath() { return GetName(); }
    int IsReadOnly();
    int IsDirectory();
    int CanRead();
    int CanWrite();
    int CanExecute();

    // Mark the file as sparse
    bool SetSparse();

    // sets the end of file to current file position
    bool SetEndOfFile();

    //allocate space on the secondary memory
    bool PreAllocate(const SV_ULONGLONG &size);

    // Flush File Buffers
    bool FlushFileBuffers();

    // Returns disk space usage of the file
    SV_ULONGLONG GetSizeOnDisk() { return GetSizeOnDisk(GetName()); }
    std::string GetBaseName() const;

    SV_LONGLONG GetSize();
    bool SetFilesystemCapabilities(const std::string &filesystem);
    bool GetFilesystemClusters(const bool &recurse, FileClusterInformer::FileClusterRanges_t &clusterranges);

    unsigned long int GetType();
    SV_ULONGLONG GetCreationTime();
    SV_ULONGLONG GetLastAccessTime();
    SV_ULONGLONG GetLastModifiedTime();
    static bool IsExisting(const std::string& sName);

    static SV_ULONGLONG GetSizeOnDisk(const std::string & fname);
    static SV_ULONGLONG GetNumberOfBytes(const std::string & fname);

    static FILE_ATTRIBUTE const File_Readable       = (FILE_ATTRIBUTE)0x0001;       // read  
    static FILE_ATTRIBUTE const File_Writable       = (FILE_ATTRIBUTE)0x0002;       // write
    static FILE_ATTRIBUTE const File_Executable     = (FILE_ATTRIBUTE)0x0004;        // execute
    static FILE_ATTRIBUTE const File_Normal         = (FILE_ATTRIBUTE)(File_Readable |File_Writable | File_Executable) ;        // execute

    static FILE_ATTRIBUTE const File_Char           = (FILE_ATTRIBUTE)0x0008;       // character file
    static FILE_ATTRIBUTE const File_Disk           = (FILE_ATTRIBUTE)0x0010;       // file is a disk
    static FILE_ATTRIBUTE const File_Pipe           = (FILE_ATTRIBUTE)0x0020;       // file is a pipe
    static FILE_ATTRIBUTE const File_Remote         = (FILE_ATTRIBUTE)0x0040;       // remote file
    static FILE_ATTRIBUTE const File_System         = (FILE_ATTRIBUTE)0x0080;       // system file
    static FILE_ATTRIBUTE const File_Hidden         = (FILE_ATTRIBUTE)0x0100;       // create if not exists, else overwrite existing
    static FILE_ATTRIBUTE const File_Removable      = (FILE_ATTRIBUTE)0x0200;       // removable file/disk/media
    static FILE_ATTRIBUTE const File_Fixed          = (FILE_ATTRIBUTE)0x0400;       // fixed.(DAS)
    static FILE_ATTRIBUTE const File_CdRom          = (FILE_ATTRIBUTE)0x0800;       // cdrom
    static FILE_ATTRIBUTE const File_RamDisk        = (FILE_ATTRIBUTE)0x1000;       // ramdisk.
    static FILE_ATTRIBUTE const File_Unknown        = (FILE_ATTRIBUTE)0x2000;       // help!!!
    static FILE_ATTRIBUTE const File_Sparse         = (FILE_ATTRIBUTE)0x4000;       // sparse file
    static FILE_ATTRIBUTE const File_Compressed     = (FILE_ATTRIBUTE)0x8000;       // compressed file
    static FILE_ATTRIBUTE const File_Directory      = (FILE_ATTRIBUTE)0x10000;       // directory file
    static FILE_ATTRIBUTE const File_ReadOnly       = (FILE_ATTRIBUTE)0x20000;       // read  only file

    int IsSystemFile();


protected:
    File();
    unsigned long int m_uliAttribute;
    std::string m_sFileSystem;
    FileClusterInformer *m_pFileClusterInformer;

private:
    SV_ULONGLONG  m_ullCreationTime;
    int Init();
    static Lockable m_ExistLock;
};
#endif

