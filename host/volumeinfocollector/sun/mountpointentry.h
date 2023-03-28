//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : mountpointentry.h
// 
// Description: 
// 

#ifndef MOUNTPOINTENTRY_H
#define MOUNTPOINTENTRY_H

#include <fcntl.h>
#include <string>
#include <cstdio>
#include <sys/mnttab.h>
#include <sys/vfstab.h>
#include "portablehelpersminor.h"
class MountPointEntryFile {
public:
    MountPointEntryFile() : m_MntFile(0) {}

    MountPointEntryFile(char const * fileName)
        {
            if (-1 == Open(fileName)) {
                // MAYBE: throw exception?
            }            
        }
    
    MountPointEntryFile(std::string const & fileName)
        {
            if (-1 == Open(fileName.c_str())) {
                // MAYBE: throw exception?
            }
        }
    
    ~MountPointEntryFile()
        {
            Close();
        }
    
    int Open(char const * fileName)
        {
            m_FileName = fileName;
            m_MntFile = fopen(fileName, "r");
            if (0 == m_MntFile) {
                return -1;
            }
        }
    
    int Open(std::string const & fileName)
        {
            return Open(fileName.c_str());
        }
    
    int Close()
        {
            if (0 != m_MntFile) {
                fclose(m_MntFile);
                m_MntFile = 0;
            }
        }

    std::string GetFileName(void)
    {
        return m_FileName;
    }
    
    virtual int Next() = 0;

    virtual char const * const MountedResource() = 0;
    virtual char const * const MountPoint() = 0;
    virtual char const * const FileSystem() = 0;
    virtual char const * const Options() = 0;
    

protected:
    FILE* m_MntFile;
    std::string m_FileName;

private:
    
};

// sun uses different apis and data structures for processing mnttab and vsftab
// so we have to have specific implementations for each

class MountTableEntry : public MountPointEntryFile {
public:
    MountTableEntry() : MountPointEntryFile("/etc/mnttab")
        {
            memset(&m_Ent, 0, sizeof(&m_Ent));
        }
    
    virtual int Next()
            {
		
            if (0 == m_MntFile) {
                return -1;
            }
            return SVgetmntent(m_MntFile, &m_Ent,m_buffer,sizeof(m_buffer));
            }
    
    virtual char const * const MountedResource()
        {
            return m_Ent.mnt_special;
        }
    
    virtual char const * const MountPoint()
        {
            return m_Ent.mnt_mountp;
        }

    virtual char const * const FileSystem()
        {
            return m_Ent.mnt_fstype;
        }

    virtual char const * const Options()
        {
            return m_Ent.mnt_mntopts;
        }
 
    

protected:

private:
    mnttab m_Ent;
	char m_buffer[MAXPATHLEN*4];
};

class FsTableEntry : public MountPointEntryFile {
public:
    FsTableEntry() : MountPointEntryFile("/etc/vfstab")
        {
            memset(&m_Ent, 0, sizeof(&m_Ent));
        }
    
    virtual int Next()
            {
            if (0 == m_MntFile) {
                return -1;
            }
            return getvfsent(m_MntFile, &m_Ent);
        }
    
    virtual char const * const MountedResource()
        {
            return m_Ent.vfs_special;
        }

    virtual char const * const MountPoint()
        {
            return m_Ent.vfs_mountp;
        }

    virtual char const * const FileSystem()
        {
            return m_Ent.vfs_fstype;
        }

    virtual char const * const Options()
        {
            return m_Ent.vfs_mntopts;
        }

protected:

private:
    vfstab m_Ent;

};


#endif // ifndef MOUNTPOINTENTRY_H
