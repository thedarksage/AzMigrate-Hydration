//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : mntpntent.h
// 
// Description: 
// 

#ifndef MNTPNTENT_H
#define MNTPNTENT_H


#include <mntent.h>
#include <fstab.h>
#include <fcntl.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <sys/param.h>

class MountPointEntryFile {
public:
    MountPointEntryFile() : m_MntFile(0)
        {
            memset(&m_Ent, 0, sizeof(m_Ent));
        }

    MountPointEntryFile(char const * fileName)
        {
            if (-1 == Open(fileName)) {
                std::string errMsg;
                errMsg = "MountPointEntryFile: Failed to open " + std::string(fileName);
                errMsg += " with error " + std::string(strerror(errno));
                throw std::runtime_error(errMsg.c_str());
            }
            
        }
    
    MountPointEntryFile(std::string const & fileName)
        {
            if (-1 == Open(fileName.c_str())) {
                std::string errMsg;
                errMsg = "MountPointEntryFile: Failed to open " + fileName;
                errMsg += " with error " + std::string(strerror(errno));
                throw std::runtime_error(errMsg.c_str());
            }
        }
    
    ~MountPointEntryFile()
        {
            Close();
        }
    
    int Open(char const * fileName)
        {
            m_FileName = fileName;
            m_MntFile = setmntent(fileName, "r");
            if (0 == m_MntFile) {
                return -1;
            }
            return 0;
        }
    
    int Open(std::string const & fileName)
        {
            return Open(fileName.c_str());
        }
    
    int Close()
        {
            if (0 != m_MntFile) {
                endmntent(m_MntFile);
                m_MntFile = 0;
            }
            return 0;
        }
    
    int Next()
        {
            if (0 != m_MntFile) {
                return (0 == getmntent_r(m_MntFile, &m_Ent, m_Buffer, sizeof(m_Buffer)) ? -1 : 0);
            }
            
            return -1;
        }
    
    char const * const MountedResource()
        {
            return m_Ent.mnt_fsname;
        }

    char const * const MountPoint()
        {
        return m_Ent.mnt_dir;
        }

    char const * const FileSystem()
        {
            return m_Ent.mnt_type;
        }

    char const * const Options()
        {
            return m_Ent.mnt_opts;
        }

    std::string GetFileName(void)
    {
        return m_FileName;
    }

protected:

private:
    FILE* m_MntFile;

    mntent m_Ent;

    char m_Buffer[MAXPATHLEN*4];

    std::string m_FileName;
};


// linux uses the same apis and data structures for processing
// mtab, /proc/mounts, and fstab so we just need simple
// classes to use the correct file

class MountTableEntry : public MountPointEntryFile {
public:
    MountTableEntry() : MountPointEntryFile("/etc/mtab")
        { }
};


class ProcMountsEntry : public MountPointEntryFile {
public:
    ProcMountsEntry() : MountPointEntryFile("/proc/mounts")
        { }
};

class FsTableEntry : public MountPointEntryFile {
public:
    FsTableEntry() : MountPointEntryFile("/etc/fstab")
        { }
};

#endif // ifndef MNTPNTENT_H
