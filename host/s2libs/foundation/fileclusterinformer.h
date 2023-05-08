#ifndef FILE_CLUSTER_INFORMER_H
#define FILE_CLUSTER_INFORMER_H

#include <string>
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "inmdefines.h"
#include "filesystem.h"

class FileClusterInformer
{
public:
    typedef struct FileDetails
    {
        ACE_HANDLE m_Handle;         /* file handle */
        std::string m_FileSystem;    /* filesystem of file */ 

        FileDetails():m_Handle(ACE_INVALID_HANDLE)
        {
        }

        FileDetails(const ACE_HANDLE &h, const std::string &filesystem):
        m_Handle(h),
        m_FileSystem(filesystem)
        {
        }
    } FileDetails_t;

    typedef struct FileClusterRanges
    {
        FeatureSupportState_t m_FeatureSupportState;
        FileSystem::ClusterRanges_t m_ClusterRanges;
 
        FileClusterRanges():
         m_FeatureSupportState(E_FEATURECANTSAY)
        {
        }

        void Print(void);

    } FileClusterRanges_t;

public:
    /* returns false on failure for now */
    virtual bool Init(const FileDetails_t &fd) = 0;

    /* returns false on failure for now */
    virtual bool GetRanges(FileClusterRanges_t &ranges) = 0;
    virtual std::string GetErrorMessage(void) = 0;
    virtual ~FileClusterInformer() {}
};

#endif /* FILE_CLUSTER_INFORMER_H */
