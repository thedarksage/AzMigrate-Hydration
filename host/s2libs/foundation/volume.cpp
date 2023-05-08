/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : volume.cpp
*
* Description: 
*/
#include "portablehelpers.h"
#include "entity.h"

#include "error.h"
#include "synchronize.h"

#include "genericfile.h"
#include "volume.h"
#include "boost/shared_array.hpp"
#include "volumeclusterinfo.h"
#include "volumeclusterinfoimp.h"
#include "sharedalignedbuffer.h"

/***********************************************************************************
*** Note: Please write any platform specific code in ${platform}/volume_port.cpp ***
***********************************************************************************/

Lockable Volume::m_FreeByteLock;
Lockable Volume::m_TotalByteLock;

/*
* FUNCTION NAME : Volume
*
* DESCRIPTION : default constructor
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
Volume::Volume() : 
m_pVolumeClusterInformer(0)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : Volume
*
* DESCRIPTION : copy constructor
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
Volume::Volume(const Volume& Vol):File(Vol),
m_pVolumeClusterInformer(0)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if(SV_SUCCESS !=  Init())
    {
        m_bInitialized = false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : Volume
*
* DESCRIPTION : constructor
*
* INPUT PARAMETERS : Volume name and mount point as std::strings
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
Volume::Volume(	
               const std::string& sVolumeName,
               const std::string& sMountPoint
               )
               :m_sMountPoint(sMountPoint),
               m_pVolumeClusterInformer(0), 
               File(sVolumeName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if(SV_SUCCESS != Init())
    {
        m_bInitialized = false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : ~Volume
*
* DESCRIPTION : destructor
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
Volume::~Volume()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (m_pVolumeClusterInformer)
    {
        delete m_pVolumeClusterInformer;
        m_pVolumeClusterInformer = 0;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : operator!=
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
bool Volume::operator!=(const Volume& rhs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bEqual = true;

    // PENDING: Implement & also invoke base class inequality operator
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bEqual;
}

/*
* FUNCTION NAME : operator==
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
bool Volume::operator==(const Volume& rhs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bEqual = true;

    // PENDING: Implement & also invoke base class equality operator
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bEqual;
}

/*
* FUNCTION NAME : GetFormattedVolumeSize
*
* DESCRIPTION : Returns the total bytes in the volume (not the free space).
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES : 
* 
* return value : 0 returned upon failure.
*
*/
SV_LONGLONG Volume::GetFormattedVolumeSize(const std::string& sVolume)
{
    SV_LONGLONG capacity = 0;

    if (GetFsVolSize(sVolume.c_str(), (unsigned long long *)&capacity).failed()) {
        capacity = 0;
    }
    return( capacity );
}


/*
* FUNCTION NAME : IsReady
*
* DESCRIPTION : returns true if the volume is online
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :turns out that for cluster volumes that are offline, you can sometimes
* still open them but won't be able to read from them this is a check to see if
* the volume is really ready
*
* return value : 
*
*/
bool Volume::IsReady()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);

    /* 
    * FIXME:
    * should tie into cluster and/or device monitor to determine if the volume is online/offline
    * for now this will work
    */
    m_bInitialized = true;
    if ( SV_SUCCESS != Open(BasicIo::BioRead | BasicIo::BioOpen | BasicIo::BioBinary | BasicIo::BioShareRW | BasicIo::BioDirect))//direct io for fabric cases.
    {
		DebugPrintf(SV_LOG_ERROR, "Failed to open volume %s with error number %lu to find if it is ready\n", GetName().c_str(), LastError());
        m_bInitialized = false;
    }
    else
    {
		/* although 4096 works for 512 sector sized drive too,
		 * get sector size using GetDiskFreeSpace api */
		const unsigned int MAX_SECTOR_SIZE = 4096;
		SharedAlignedBuffer buffer(MAX_SECTOR_SIZE, MAX_SECTOR_SIZE); 
        SV_UINT readbytes = Read(buffer.Get(), MAX_SECTOR_SIZE );
        if (Good()) 
			DebugPrintf(SV_LOG_DEBUG, "Read %u bytes from volume %s, when asked for %u bytes\n", readbytes, GetName().c_str(), MAX_SECTOR_SIZE);
		else {
			DebugPrintf(SV_LOG_ERROR, "Failed to read %u bytes on volume %s with error number %lu to find if it is ready\n", MAX_SECTOR_SIZE, GetName().c_str(), LastError());
            m_bInitialized = false;
        }
        Close();
        if (!Good())
        {
			DebugPrintf(SV_LOG_ERROR, "Failed to close volume %s with error number %lu to find if it is ready.\n", GetName().c_str(), LastError());
            m_bInitialized = false;
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return m_bInitialized;
}

/*
* FUNCTION NAME : Hide
*
* DESCRIPTION : hides the volume
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
bool Volume::Hide()
{ 
    std::string name = GetName();

    FormatVolumeNameToGuid(name);        

	VOLUME_STATE state = GetVolumeState( GetName().c_str());

    /*if (IsVolumeLocked(name.c_str())) {
        return true;
    }*/

	if(state == VOLUME_HIDDEN)
	{
        return true;
    }

    std::string sMountPoint = GetMountPoint();
    if ( sMountPoint.empty() )
        sMountPoint = GetName();
    if (HideDrive(name.c_str(),sMountPoint.c_str()).failed()) 
    {
        return false;
    }

    /* 
    * HACK:
    * for some reason on some setups, the HideDrive does not
    * keep the drive hidden. So we do this extra 
    * open, close exclusive to get the system to flush things after the hide
    */
	if(state != VOLUME_UNKNOWN) //16211 - For raw volumes don't call OpenVolumeExclusive.
	{
    ACE_HANDLE h;
    std::string sguid;
    SVERROR sve = OpenVolumeExclusive(&h, sguid, name.c_str(), sMountPoint.c_str(), TRUE, FALSE);
    if (sve.failed()) 
    {
        return false;
    }
    CloseVolumeExclusive(h, sguid.c_str(), name.c_str(), sMountPoint.c_str());
	}
    return true; 

}

/*
* FUNCTION NAME : OpenExclusive
*
* DESCRIPTION : Opens the volume in exclusive mode
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
bool Volume::OpenExclusive()
{
    bool rv = true;

    do
    {
        if(!isOpen())
        {
            if(!Hide())
            {
                rv = false;
                break;
            }

            BasicIo::BioOpenMode mode = (BasicIo::BioRWExisitng | BasicIo::BioShareAll | BasicIo::BioBinary);
#ifdef SV_WINDOWS
            mode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
            mode |= BasicIo::BioDirect;
#endif
            Open(mode);

            if (!Good())
            {
                rv = false;
                break;
            }
        }

        rv = true;
    } while (false);

    return rv;
}

/*
* FUNCTION NAME : Open
*
* DESCRIPTION : Opens the volume in the mode specified
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
SV_INT Volume::Open(BasicIo::BioOpenMode mode)
{
    std::string volume = GetName();
    FormatVolumeNameToGuid( volume ); 

    return m_Bio.Open(volume.c_str(), mode);
}


/*
* FUNCTION NAME : IsFiltered
*
* DESCRIPTION : returns true if the volume is filtered
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
bool Volume::IsFiltered()
{
    return m_bIsFilteredVolume;
}

/*
* FUNCTION NAME : VolumeGUIDLength
*
* DESCRIPTION : returns the length of the volumeguid
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
int Volume::VolumeGUIDLength() const 
{ 
    /* BUGBUG: return the length in no of characters and not the size. */
    return sizeof(m_sVolumeGUID); 
}

/*
* FUNCTION NAME : GetMountPoint
*
* DESCRIPTION : returns the mount point as a string
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
const std::string& Volume::GetMountPoint() const
{
    return m_sMountPoint;
}

/*
* FUNCTION NAME : LastError
*
* DESCRIPTION : returns getlasterror in windows and errno in linux platforms
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
SV_ULONG Volume::LastError() const
{
    SV_ULONG nativeError = File::LastError();

    if(0 == nativeError)
    {
#ifdef SV_WINDOWS
        nativeError = GetLastError();
#else
        nativeError = errno;
#endif
    }

    return nativeError;
}


FeatureSupportState_t Volume::SetFileSystemCapabilities(const std::string &filesystem)
{
    FeatureSupportState_t fs = E_FEATURECANTSAY;

    if (m_pVolumeClusterInformer)
    {
        fs = m_pVolumeClusterInformer->GetSupportState();
    }
    else
    {
        m_sFileSystem = filesystem;
        m_pVolumeClusterInformer = new (std::nothrow) VolumeClusterInformerImp();
        if (m_pVolumeClusterInformer)
        {
            VolumeClusterInformer::VolumeDetails vd(GetHandle(), m_sFileSystem);
            if (m_pVolumeClusterInformer->Init(vd))
            {
                fs = m_pVolumeClusterInformer->GetSupportState();
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "could not initialize volume cluster information for volume %s\n", 
                                          GetName().c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "could not allocate memory for volume cluster information for volume %s\n", 
                                      GetName().c_str());
        }
    }

    return fs;
}


bool Volume::SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset)
{
    bool bisset = false;

    if (m_pVolumeClusterInformer)
    {
        SV_UINT minimumclustersizepossible = GetPhysicalSectorSize();
        if (minimumclustersizepossible)
        {
            bisset = m_pVolumeClusterInformer->InitializeNoFsCluster(size, startoffset, minimumclustersizepossible);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "SetNoFileSystemCapabilities cannot be done because "
                                      "physical sector size could not be found for volume %s\n", 
                                      GetName().c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "SetNoFileSystemCapabilities cannot be called without "
                                  "determining file system support capabilities for volume %s\n", 
                                  GetName().c_str());
    }

    return bisset;
}


SV_ULONGLONG Volume::GetNumberOfClusters(void)
{
    SV_ULONGLONG nclus = 0;
    if (m_pVolumeClusterInformer)
    {
        nclus = m_pVolumeClusterInformer->GetNumberOfClusters();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For volume %s, file system capabilities is uninitialized but asked\n", GetName().c_str());
    }
 
    return nclus;
}


bool Volume::GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci)
{
    bool brval = false;
    if (m_pVolumeClusterInformer)
    {
        brval = m_pVolumeClusterInformer->GetVolumeClusterInfo(vci);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For volume %s, file system capabilities is uninitialized but asked\n", GetName().c_str());
    }
 
    return brval;
}


void Volume::PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci)
{
    if (m_pVolumeClusterInformer)
    {
        m_pVolumeClusterInformer->PrintVolumeClusterInfo(vci);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For volume %s, file system capabilities is uninitialized but asked\n", GetName().c_str());
    }
}


SV_UINT Volume::GetPhysicalSectorSize(void)
{
    const int MINBYTES = 512;
    const int MAXBYTES = 4096;
    SV_UINT s = 0;
#ifdef SV_UNIX
    boost::shared_array<char> maxbuf((char*) valloc(MAXBYTES), free); //aligned buffer for direct io.
#else
    boost::shared_array<char> maxbuf(new (std::nothrow) char[MAXBYTES]);
#endif

    if (0 == maxbuf.get())
    {
        DebugPrintf(SV_LOG_ERROR, "Allocating %d bytes to find physical sector size for volume %s failed\n",
                                  MAXBYTES, GetName().c_str());
        return 0;
    }

    SV_LONGLONG savepos = Tell();
    Seek(0, BasicIo::BioBeg);
    SV_UINT bytesread = FullRead(maxbuf.get(), MINBYTES);
    if(MINBYTES == bytesread)
    {
        s = bytesread;
    }
    else 
    {
        DebugPrintf(SV_LOG_DEBUG, "For volume %s, failed to read %d bytes with error %lu to get physical sector size. "
                                  "Trying to read %d bytes\n", GetName().c_str(), MINBYTES, LastError(), MAXBYTES);
        bytesread = FullRead(maxbuf.get(), MAXBYTES);
        if(MAXBYTES == bytesread)
        {
            s = bytesread;
        }
        else 
        {
            DebugPrintf(SV_LOG_ERROR, "For volume %s, failed to read %d bytes with error %lu to get physical sector size. "
                                      "volume is offline\n", GetName().c_str(), MAXBYTES, LastError());
        }
    }
    Seek(savepos, BasicIo::BioBeg);

    DebugPrintf(SV_LOG_DEBUG, "physical sector size for volume %s is %u\n", GetName().c_str(), s);
    return s;
}


SV_ULONGLONG Volume::GetSize(void)
{
	return GetFormattedVolumeSize(GetName());
}