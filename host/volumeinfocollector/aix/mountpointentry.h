#ifndef MOUNTPOINTENTRY_H
#define MOUNTPOINTENTRY_H

#include <fcntl.h>
#include <string>
#include <cstdio>

#include <sys/types.h>
#include <sys/mntctl.h>
#include <sys/vmount.h>
#include <stdlib.h>
#include "portablehelpersminor.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

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

private:
	std::string m_FileName ;    
};

// sun uses different apis and data structures for processing mnttab and vsftab
// so we have to have specific implementations for each

class MountTableEntry : public MountPointEntryFile {
public:
    MountTableEntry() 
        {
            int size;
            char * buf;
            struct vmount * vm;
            int rdonly = 0;
            int num;
            int fsnameLength;
            int i, numFilesystems ;
            num = mntctl(MCTL_QUERY, sizeof(size), (char *) &size);
            if (num < 0)
            {
 				DebugPrintf(SV_LOG_ERROR, "Failed to get the size %d\n",errno) ;
            }
            INM_SAFE_ARITHMETIC(size *= InmSafeInt<int>::Type(2), INMAGE_EX(size))
            buf = (char *)malloc(size);
            num = mntctl(MCTL_QUERY, size, buf);
            if ( num <= 0 )
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to get the mount points info %d\n", errno) ;
            }
            numFilesystems = num;
            size_t fsnlen;
            for (vm = ( struct vmount *)buf, i = 0; i < num; i++)
            {
                mount_info mInfo;
                char *fsn;
                fsnameLength = vm->vmt_data[VMT_OBJECT].vmt_size;
                INM_SAFE_ARITHMETIC(fsnlen = InmSafeInt<int>::Type(fsnameLength) + 1, INMAGE_EX(fsnameLength))
                fsn = (char*)malloc(fsnlen);
                inm_strncpy_s(fsn, fsnameLength + 1, (char *)vm + vm->vmt_data[VMT_OBJECT].vmt_off, fsnameLength);
                mInfo.m_deviceName = fsn;
                if(fsn)
                {
                    free(fsn);
                }
                fsn = NULL;
                fsnameLength = vm->vmt_data[VMT_STUB].vmt_size;
                INM_SAFE_ARITHMETIC(fsnlen = InmSafeInt<int>::Type(fsnameLength) + 1, INMAGE_EX(fsnameLength))
                fsn = (char*)malloc(fsnlen);
                inm_strncpy_s(fsn, fsnameLength + 1, (char *)vm + vm->vmt_data[VMT_STUB].vmt_off, fsnameLength);
                mInfo.m_mountedOver = fsn;
                if(fsn)
                {
                    free(fsn);
                }
                fsn = NULL;
                fsnameLength = vm->vmt_data[VMT_ARGS].vmt_size;
                INM_SAFE_ARITHMETIC(fsnlen = InmSafeInt<int>::Type(fsnameLength) + 1, INMAGE_EX(fsnameLength))
                fsn = (char*)malloc(fsnlen);
                inm_strncpy_s(fsn, fsnameLength + 1, (char *)vm + vm->vmt_data[VMT_ARGS].vmt_off, fsnameLength);
                mInfo.m_options = fsn;
                if(fsn)
                {
                    free(fsn);
                }
                fsn = NULL;
                fsnameLength = vm->vmt_data[VMT_HOSTNAME].vmt_size;
                INM_SAFE_ARITHMETIC(fsnlen = InmSafeInt<int>::Type(fsnameLength) + 1, INMAGE_EX(fsnameLength))
                fsn = (char*)malloc(fsnlen);
                inm_strncpy_s(fsn, fsnameLength + 1, (char *)vm + vm->vmt_data[VMT_HOSTNAME].vmt_off, fsnameLength);
                mInfo.m_nodeName = fsn;
                if(fsn)
                {
                    free(fsn);
                }
				mInfo.m_vfs = getFSString(vm->vmt_gfstype);
                mountInfoList.push_back(mInfo);
				DebugPrintf(SV_LOG_DEBUG, "Device Name %s\n", mInfo.m_deviceName.c_str()) ;
				//std::cout<< "Mount point name is " << mInfo.m_deviceName << std::endl ;
                vm = (struct vmount *)((char *)vm + vm->vmt_length);
            }
			m_begin = mountInfoList.begin() ;
			m_end = mountInfoList.end() ;
			//std::cout<<"No of mount points are "<< mountInfoList.size() ;
	
        }
    
    virtual int Next()
    {
		if( m_begin != m_end )
		{
			
			m_current = *m_begin ;
			m_begin++ ;
        	return 0;
        }
		else
		{
			return -1; 
		}
    }
    
    virtual char const * const MountedResource()
    {
    	return m_current.m_deviceName.c_str()  ;
    }
    
    virtual char const * const MountPoint()
    {
	    return m_current.m_mountedOver.c_str() ;
    }

    virtual char const * const FileSystem()
    {
    	return  m_current.m_vfs.c_str() ;
    }

    virtual char const * const Options()
    {
    	return m_current.m_options.c_str() ;
    }

protected:

private:
	std::list<mount_info> mountInfoList ;
	std::list<mount_info>::iterator m_begin, m_end ;
	mount_info m_current ;
};

class FsTableEntry : public MountPointEntryFile {
public:
    FsTableEntry() : MountPointEntryFile("/etc/vfstab")
        {
            //memset(&m_Ent, 0, sizeof(&m_Ent));
        }
    
    virtual int Next()
            {
            if (0 == m_MntFile) {
                return -1;
            }
            return -1;//getvfsent(m_MntFile, &m_Ent);
        }
    
    virtual char const * const MountedResource()
        {
            return "";//m_Ent.vfs_special;
        }

    virtual char const * const MountPoint()
        {
            return "";//m_Ent.vfs_mountp;
        }

    virtual char const * const FileSystem()
        {
            return "";//m_Ent.vfs_fstype;
        }

    virtual char const * const Options()
        {
            return "";//m_Ent.vfs_mntopts;
        }

protected:

private:
    //vfstab m_Ent;

};


#endif // ifndef MOUNTPOINTENTRY_H

