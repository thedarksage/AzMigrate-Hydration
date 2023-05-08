#include <sstream>
#include <string>
#include <cstdlib>
#include "ntfsclusterinfo.h"
#include "portablehelpers.h"
#include "logger.h"
#include "inmsafeint.h"
#include "inmageex.h"

    
NtfsClusterInfo::NtfsClusterInfo()
{
    m_StartingLcn.StartingLcn.QuadPart = 0;
}


NtfsClusterInfo::NtfsClusterInfo(const VolumeClusterInformer::VolumeDetails &vd):
m_Vd(vd)
{
    m_StartingLcn.StartingLcn.QuadPart = 0;
}


SV_ULONGLONG NtfsClusterInfo::GetNumberOfClusters(void)
{
    m_StartingLcn.StartingLcn.QuadPart = 0;
    VOLUME_BITMAP_BUFFER volumebitmapbuf;
    DWORD bytesreturned = 0;
    SV_ULONGLONG nclusters = 0;

    memset(&volumebitmapbuf, 0, sizeof volumebitmapbuf);
    BOOL b = DeviceIoControl(m_Vd.m_Handle, FSCTL_GET_VOLUME_BITMAP, &m_StartingLcn,
        sizeof m_StartingLcn, &volumebitmapbuf, sizeof volumebitmapbuf,
        &bytesreturned, NULL);
    bool bgotinfo = false;
    if (b)
    {
        DebugPrintf(SV_LOG_DEBUG, "success in getting fs bitmap\n");
        bgotinfo = true;
    }
    else
    {
        DWORD err = GetLastError();
        if (ERROR_MORE_DATA == err)
        {    
            DebugPrintf(SV_LOG_DEBUG, "FSCTL_GET_VOLUME_BITMAP says error more data\n");
            bgotinfo = true;
        }
        else
        {
            std::stringstream sserr;
            sserr << err;
            DebugPrintf(SV_LOG_ERROR, "FSCTL_GET_VOLUME_BITMAP failed with %s\n", sserr.str().c_str());
        }    
    }

    if (bgotinfo)
    {
        nclusters = volumebitmapbuf.BitmapSize.QuadPart;
        DebugPrintf(SV_LOG_DEBUG, "number of clusters = " ULLSPEC "\n", nclusters);
    }

    return nclusters;
}


bool NtfsClusterInfo::GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci)
{
    /* NOTE: From msdn:
     * The BitmapSize member is the number of clusters on the volume starting 
     * from the starting LCN returned in the StartingLcn member of this structure. 
     * For example, suppose there are 0xD3F7 clusters on the volume. If you 
     * start the bitmap query at LCN 0xA007, then both the FAT and NTFS file systems 
     * will round down the returned starting LCN to LCN 0xA000. 
     * The value returned in the BitmapSize member will be (0xD3F7 . 0xA000), or 0x33F7.
     *
     * Hence vci.m_Start also will be modified with what ever is returned in
     * StartingLcn member of VOLUME_BITMAP_BUFFER; No need to modify m_StartingLcn.StartingLcn.QuadPart 
     * since it will be rounded down */

    std::stringstream msg;
    msg << "Asked ntfs cluster start: " << vci.m_Start
        << ", count: " << vci.m_CountRequested;

    /* But to be on safe side, ask from nearest lowest multiple of 8; 
     * also increase the length */
    SV_ULONGLONG rem;
    INM_SAFE_ARITHMETIC(rem = InmSafeInt<SV_ULONGLONG>::Type(vci.m_Start)%NBITSINBYTE, INMAGE_EX(vci.m_Start))
    vci.m_Start -= rem;
    INM_SAFE_ARITHMETIC(vci.m_CountRequested += InmSafeInt<SV_ULONGLONG>::Type(rem), INMAGE_EX(vci.m_CountRequested)(rem))

    msg << ". Adjusted (aligned to 8) ntfs cluster start for query: " << vci.m_Start
        << ", length " << vci.m_CountRequested;
    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());

    m_StartingLcn.StartingLcn.QuadPart = vci.m_Start;
    SV_UINT mod;
    INM_SAFE_ARITHMETIC(mod = InmSafeInt<SV_UINT>::Type(vci.m_CountRequested) % NBITSINBYTE, INMAGE_EX(vci.m_CountRequested))
    SV_UINT add = mod ? 1 : 0;
	size_t nbytesforbitmap;
    INM_SAFE_ARITHMETIC(nbytesforbitmap = (InmSafeInt<SV_UINT>::Type(vci.m_CountRequested) / NBITSINBYTE) + add, INMAGE_EX(vci.m_CountRequested)(add))
    size_t outsize;
    INM_SAFE_ARITHMETIC(outsize = sizeof (VOLUME_BITMAP_BUFFER) + ((InmSafeInt<size_t>::Type(nbytesforbitmap) - 1) * sizeof (BYTE)), INMAGE_EX(sizeof (VOLUME_BITMAP_BUFFER))(nbytesforbitmap)(sizeof (BYTE)))
	bool bgotinfo = false;

    if (AllocateBitmapIfReq(outsize, vci))
    {
		PVOLUME_BITMAP_BUFFER pbitmapbuffer = (PVOLUME_BITMAP_BUFFER)vci.GetAllocatedData();
        DWORD bytesreturned = 0;
        BOOL b = DeviceIoControl(m_Vd.m_Handle, FSCTL_GET_VOLUME_BITMAP, &m_StartingLcn,
            sizeof m_StartingLcn, pbitmapbuffer, vci.GetAllocatedLength(),
            &bytesreturned, NULL);

        if (b)
        {
            DebugPrintf(SV_LOG_DEBUG, "success in getting fs bitmap\n");
            vci.m_CountFilled = pbitmapbuffer->BitmapSize.QuadPart;
			if (vci.m_CountFilled > vci.m_CountRequested)
			{
				/* 
                 * This is needed because size of VOLUME_BITMAP_BUFFER 
				 * has some padding to make it multiple of 8.
				 * Hence Buffer[2] is from padded space 
				 * resulting in 7 bytes extra allocation 
				 */
				vci.m_CountFilled = vci.m_CountRequested;
			}
            bgotinfo = true;
        }
        else
        {
            DWORD err = GetLastError();
            if (ERROR_MORE_DATA == err)
            {    
                DebugPrintf(SV_LOG_DEBUG, "FSCTL_GET_VOLUME_BITMAP says error more data\n");
                vci.m_CountFilled = vci.m_CountRequested;
                bgotinfo = true;
            }
            else
            {
                std::stringstream sserr;
                sserr << err;
                DebugPrintf(SV_LOG_ERROR, "FSCTL_GET_VOLUME_BITMAP failed with %s\n", sserr.str().c_str());
            }    
        }

        if (bgotinfo)
        {
            vci.m_pBitmap = pbitmapbuffer->Buffer;
            /* TODO: remove this debug message 
            std::stringstream ss;
			ss << "When asked from cluster " << m_StartingLcn.StartingLcn.QuadPart
				<< ", got cluster from " << pbitmapbuffer->StartingLcn.QuadPart;
			DebugPrintf(SV_LOG_DEBUG, "%s\n", ss.str().c_str()); 
            */
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For request of %u clusters information, could not allocate memory\n", vci.m_CountRequested);
    }

    return bgotinfo;
}


bool NtfsClusterInfo::AllocateBitmapIfReq(const size_t &outsize, VolumeClusterInformer::VolumeClusterInfo &vci)
{
    bool brval = false;

	std::stringstream sstmp;
	size_t bitmapdatasize = vci.GetAllocatedLength();
	sstmp << "allocated bitmap data size  = " << bitmapdatasize
          << ", outsize = " << outsize;
	DebugPrintf(SV_LOG_DEBUG, "%s\n", sstmp.str().c_str());
    if (outsize)
    {
        if (outsize > bitmapdatasize)
        {
			vci.Release();
			brval = vci.Allocate(outsize);
			if (!brval)
			{
			    std::stringstream ss;
                ss << outsize;
                DebugPrintf(SV_LOG_ERROR, "could not allocate %s for cluster bitmap\n", ss.str().c_str());
			}
        }
        else
        {
            brval = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "size requested is zero for volume bitmap buffer\n");
    }

    return brval;
}


NtfsClusterInfo::~NtfsClusterInfo()
{
}
