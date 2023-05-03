#ifndef VOLUME__CLUSTER__INFO__H_
#define VOLUME__CLUSTER__INFO__H_

#include <string>
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "inmdefines.h"
#include "inmdefinesmajor.h"
#include "svtypes.h"

class VolumeClusterInformer
{
public:
    /* VolumeClusterInfo: interface structure for applications */
    typedef struct stVolumeClusterInfo
    {
        SV_ULONGLONG m_Start;        /* input: starting cluster */
        SV_UINT m_CountRequested;    /* input: number of clusters requested */
        SV_UINT m_CountFilled;       /* output: number of clusters filled */
        SV_BYTE *m_pBitmap;          /* bytes having bitmap; 
									  * set as a pointer in 
									  * allocated data */
		SV_BYTE *m_pAllocatedBitmapData; /* allocated bitmap data; It may have bitmap metadata based on filesystem */
		size_t m_AllocatedLength;        /* allocated length */

        stVolumeClusterInfo()
			: m_Start(0),
			  m_CountRequested(0),
			  m_CountFilled(0),
			  m_pBitmap(0),
			  m_pAllocatedBitmapData(0),
			  m_AllocatedLength(0)
        {
        }

        void Reset(void)
        {
            m_Start = 0;
            m_CountRequested = 0;
            m_CountFilled = 0;
            m_pBitmap = 0;
        }

		size_t GetAllocatedLength(void)
		{
			return m_AllocatedLength;
		}

		SV_BYTE * GetAllocatedData(void)
		{
			return m_pAllocatedBitmapData;
		}

		bool Allocate(const size_t &length)
		{
			Release();
			m_pAllocatedBitmapData = new (std::nothrow) SV_BYTE[length];
			bool allocated = false;
			if (m_pAllocatedBitmapData)
			{
				allocated = true;
				m_AllocatedLength = length;
			}
			return allocated;
		}

		void Release(void)
		{
			if (m_pAllocatedBitmapData)
			{
				delete [] m_pAllocatedBitmapData;
				m_pAllocatedBitmapData = 0;
				m_AllocatedLength = 0;
			}
		}

		~stVolumeClusterInfo()
		{
			Release();
		}

    } VolumeClusterInfo;

    typedef struct stVolumeDetails
    {
        ACE_HANDLE m_Handle;         /* volume handle */
        std::string m_FileSystem;    /* filesystem on volume */ 

        stVolumeDetails():m_Handle(ACE_INVALID_HANDLE)
        {
        }

        stVolumeDetails(const ACE_HANDLE &h, const std::string &filesystem):
        m_Handle(h),
        m_FileSystem(filesystem)
        {
        }
    } VolumeDetails;
    
public:
    /* returns false on failure for now */
    virtual bool Init(const VolumeDetails &vd) = 0;
    /* returns false on failure for now */
    virtual bool InitializeNoFsCluster(const SV_LONGLONG size, const SV_LONGLONG startoffset, const SV_UINT minimumclustersizepossible) = 0;
    /* returns 0 on failure */
    virtual SV_ULONGLONG GetNumberOfClusters(void) = 0;    
    /* returns false on failure for now */
    virtual bool GetVolumeClusterInfo(VolumeClusterInfo &vci) = 0;
    virtual FeatureSupportState_t GetSupportState(void) = 0;
    virtual ~VolumeClusterInformer() {}
    
    void PrintVolumeClusterInfo(const VolumeClusterInfo &vci);
};

#endif /* VOLUME__CLUSTER__INFO__H_ */
