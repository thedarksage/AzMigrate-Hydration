//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpsnapshot.cpp
//
// Description: 
//

#include <boost/lexical_cast.hpp>

#include "cdpsnapshot.h"
#include "error.h"

#include "cdputil.h"
#include "cdpvolume.h"
#include "cdpplatform.h"

#include "VsnapUser.h"
#include "inmcommand.h"
#include "portablehelpersmajor.h"

#include "inmageex.h"

#include "configwrapper.h"

#include "differentialfile.h"
#include "sharedalignedbuffer.h"
#include "volumeinfocollector.h"
#include "cdplock.h"
#include <fstream>
#include <iomanip>
#include <iostream>

#include "volumereporter.h"
#include "volumereporterimp.h"

#include "AgentHealthIssueNumbers.h"

using namespace std;

/*
* FUNCTION NAME :  SnapshotTask::InitializeConfigurator
*
* DESCRIPTION : initialize cofigurator to fetch initialsettings  
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool SnapshotTask::InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try 
        {
            m_configurator = NULL;
            if(!GetConfigurator(&m_configurator))
            {
                rv = false;
                break;
            }

            m_bconfig = true;
            rv = true;
        }

        catch ( ContextualException& ce )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        }
        catch( exception const& e )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, e.what());
        }
        catch ( ... )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered unknown exception.\n",
                FUNCTION_NAME);
        }

    } while(0);

    if(!rv)
    {
        DebugPrintf(SV_LOG_INFO, 
            "Replication pair information is not available in local cache.\n"
            "Few checks will be skipped.\n");
        m_bconfig = false;
    }

    return rv;
}

/*
* FUNCTION NAME :  SnapshotTask::sameVolumecheck
*
* DESCRIPTION : check if the specified volume is a protected (filtered) volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::sameVolumeCheck(const std::string & vol1, const std::string & vol2) const
{
    std::string s1 = vol1;
    std::string s2 = vol2;

    // Convert the volume name to a standard format
    FormatVolumeName(s1);
    FormatVolumeNameToGuid(s1);
    ExtractGuidFromVolumeName(s1);

    // convert to standard device name if symbolic link 
    // was passed as argument

    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(s1);
    }

    // Convert the volume name to a standard format
    FormatVolumeName(s2);
    FormatVolumeNameToGuid(s2);
    ExtractGuidFromVolumeName(s2);

    // convert to standard device name if symbolic link 
    // was passed as argument

    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(s2);
    }

    return (s1 == s2);
}

/*
* FUNCTION NAME :  SnapshotTask::isProtectedVolume
*
* DESCRIPTION : check if the specified volume is a protected (filtered) volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool SnapshotTask::isProtectedVolume(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->isProtectedVolume(volumename));
}

/*
* FUNCTION NAME :  SnapshotTask::isTargetVolume
*
* DESCRIPTION : check if the specified volume is a target volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool SnapshotTask::isTargetVolume(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->isTargetVolume(volumename));
}


/*
* FUNCTION NAME :  SnapshotTask::containsRetentionFiles
*
* DESCRIPTION : check if specified volume contains retention files 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool SnapshotTask::containsRetentionFiles(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->containsRetentionFiles(volumename));
}

/*
* FUNCTION NAME :  SnapshotTask::containsVsnapFiles
*
* DESCRIPTION : check if specified volume contains vsnap files 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::containsVsnapFiles(const std::string & volumename) const
{
    //
    // two possible implementations:
    //  1) add an ioctl to driver to check if the volume (mtpt) 
    //     passed contains any vsnap map files
    //  2) persist vsnap information in local store and check from local store
    //
    // leaving it unimplemented for now

    return false;
}

/*
* FUNCTION NAME :  SnapshotTask::containsMountedVolumes
*
* DESCRIPTION : check if specified volume contains retention files 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::containsMountedVolumes(const std::string & volumename, std::string & mountedvolume) const
{
    bool rv = false;
    std::string s1 = volumename;

#ifdef SV_WINDOWS

    FormatVolumeNameForCxReporting(s1);
    FormatVolumeNameForMountPoint(s1);   

    char MountPointBuffer[ SV_MAX_PATH+1 ];
    HANDLE hMountPoint = FindFirstVolumeMountPoint( s1.c_str(),
        MountPointBuffer, sizeof(MountPointBuffer) - 1 );
    if( INVALID_HANDLE_VALUE == hMountPoint )
    {
        rv = false;
    }
    else
    {
        rv = true;
        mountedvolume = MountPointBuffer;
        FindVolumeMountPointClose(hMountPoint);
    }
    //Bug #5685
#else
    return ::containsMountedVolumes(volumename, mountedvolume);

#endif

    return rv;
}

/*
* FUNCTION NAME :  SnapshotTask::isResyncing
*
* DESCRIPTION : check if specified volume is undergoing resync operation 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::isResyncing(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->isResyncing(volumename));
}

/*
* FUNCTION NAME :  SnapshotTask::getFSType
*
* DESCRIPTION : get the file system type on volume from configurator
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : filesystem type on the volume
*                "" if it is raw volume / information not available
*
*/
std::string SnapshotTask::getFSType(const std::string & volumename) const
{

    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return "";
    }

    return(m_configurator ->getVxAgent().getSourceFileSystem(volumename));
}

/*
* FUNCTION NAME :  SnapshotTask::computeSrcCapacity
*
* DESCRIPTION : compute the capacity of the specified volume
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
* procedure:
*  already there, leave it
*  get it from configurator
*  not available, get fs volume size,
*  fs volume size not available - fail
*
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::computeSrcCapacity(const std::string & volumename, 
                                      SV_ULONGLONG & capacity)
{
    bool bcompute = true;

    // 
    // note: 
    // if the capacity passed is non zero
    // do not overwrite it

    // Replication Pair configuration information is available
    // we will get the capacity from configurator
    //
    if(!capacity && m_bconfig)
    {
        getSourceCapacity((*m_configurator), volumename, capacity);
    }

    // 
    // if the capacity is not available with configurator
    // it returns zero
    //
    if(!capacity)
    {
        std::string sparsefile;
        bool newvirtualvolume = false; 
        IsVolPackDevice(volumename.c_str(),sparsefile,newvirtualvolume);
        capacity = cdp_volume_t::GetRawVolumeSize(volumename,newvirtualvolume);
        if(!capacity)
            capacity = cdp_volume_t::GetFormattedVolumeSize(volumename,newvirtualvolume);
    }

    if(!capacity)
        return false;

    DebugPrintf(SV_LOG_INFO, "source volume %s capacity :" ULLSPEC " bytes\n",
        volumename.c_str(),capacity);

    return true;
}

/*
* FUNCTION NAME :  SnapshotTask::computDestCapacity
*
* DESCRIPTION : compute capacity for snapshot destination
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::computDestCapacity(const std::string & volumename, 
                                      SV_ULONGLONG & capacity)
{
    // 
    // first try to get raw volume size
    // if it fails, we will try to get fs capacity
    //
    std::string sparsefile;
    bool newvirtualvolume = false; 
    IsVolPackDevice(volumename.c_str(),sparsefile,newvirtualvolume);
    capacity = cdp_volume_t::GetRawVolumeSize(volumename,newvirtualvolume);
    if(!capacity)
        capacity = cdp_volume_t::GetFormattedVolumeSize(volumename,newvirtualvolume);

    if(!capacity)
        return false;

    DebugPrintf(SV_LOG_INFO, "Destination volume %s capacity :" ULLSPEC " bytes\n",
        volumename.c_str(),capacity);
    return true;
}

/*
* FUNCTION NAME :  SnapshotTask::makeVolumeAvailable
*
* DESCRIPTION : unlock the volume
*   windows:
*     revert the svfs signature to ntfs/fat and make it readwrite
*   unix:
*     mount the volume at specified directory
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::makeVolumeAvailable(const std::string & volumename, 
                                       const std::string & mountpoint,
                                       const std::string & fsType)
{

#ifdef SV_WINDOWS

    if(UnhideDrive_RW(volumename.c_str(),volumename.c_str()).failed())
    {
        m_err += "unlock in read write mode failed for";
        m_err += volumename;
        m_errno = CDPTgtUnhideFailed;
        return false;
    }

#else
    if(!RestoreNtfsOrFatSignature(volumename))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to revert back the SVFS signature to NTFS for volume :%s \n",volumename.c_str());
    }
    if(mountpoint.empty())
    {
        m_err += "Mount point is not specified. ";
        m_err += volumename;
        m_err += " will not be mounted.\n";
        return true;
    }
    //BugId:6701 skip this checking for source volume
    if(!m_srcVolRollback){
        if(fsType == "" )
        {
            m_err += volumename;
            m_err += " will not be mounted as the source file system is raw or unspecified.";
            m_err += "Please complete the mount operation manually.\n";
            return true;
        }
    }

    if(!stricmp(fsType.c_str(),"ntfs"))
    {
        m_err += volumename;
        m_err += " will not be mounted as the source file system is ntfs.";
        m_err += "You can export the device manually.\n";
        return true;
    }

    if(SVMakeSureDirectoryPathExists( mountpoint.c_str()).failed())
    {
        m_err += "specified mountpoint:";
        m_err += mountpoint;
        m_err += "could not be created.\n";
        m_err += volumename;
        m_err += "will not be mounted.\n";
        m_errno = CDPTgtUnhideFailed;
        return false;
    }

    if(UnhideDrive_RW(volumename.c_str(),  mountpoint.c_str(),fsType.c_str()).failed())
    {
        m_err += "mount operation failed";
        m_err += "specified parameters:\n";
        m_err += "FileSystem: ";
        m_err += fsType;
        m_err += "\nMount Point: ";
        m_err += mountpoint;
        m_err += "\n Device: ";
        m_err += volumename;
        m_errno = CDPTgtUnhideFailed;
        return false;
    }
#endif

    return true;
}

/*
* FUNCTION NAME :  SnapshotTask::copydata
*
* DESCRIPTION : perform volume copy
*
* INPUT PARAMETERS : source volume handle
*                    target volume handle
*                    bytes to copy
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool SnapshotTask::copydata(cdp_volume_t & srcvol, cdp_volume_t & tgtvol, const SV_ULONGLONG & capacity)
{

    SV_UINT const MAX_RW = m_cdp_max_snapshot_rw_size;
    SV_ULONGLONG cbRemaining = capacity;
    SV_ULONG progress = 0;

    ACE_Time_Value startTime = ACE_OS::gettimeofday(); 
    ACE_Time_Value presentTime = ACE_OS::gettimeofday();
    m_time_remaining_in_secs = 0;

    SharedAlignedBuffer pbuffer(MAX_RW, m_sector_size);
    if( !pbuffer )
    {
        m_err += "Error: insufficient memory to allocate buffers\n";
        m_errno = CDPOUTOFMEMORY;
        return false;
    }

    // Transfer the contents from source volume to target volume...
    while (!Quit() && cbRemaining ) 
    {
        SV_UINT cbToRead = (SV_UINT) min( cbRemaining, (SV_ULONGLONG)MAX_RW );
        SV_UINT cbread   = srcvol.FullRead(pbuffer.Get(), cbToRead);

        if( (cbread != cbToRead || !srcvol.Good()) && !srcvol.Eof())
        {
            stringstream msg;
            msg << "Error during read operation on volume" << srcvol.GetName() << "\n"
                << " Offset : " << srcvol.Tell()<< "\n"
                << "Expected Read Bytes:" << cbToRead << "\n"
                << "Actual Read Bytes:"   << cbread << "\n"
                << "Error Code: " << srcvol.LastError() << "\n"
                << "Error Message: " << Error::Msg(srcvol.LastError()) << "\n";
            m_err += msg.str();
            m_errno = CDPReadFromSrcFailed;
            break;
        }
#ifdef SV_WINDOWS
        if(0 == tgtvol.Tell())
        {
            HideBeforeApplyingSyncData(pbuffer.Get(), pbuffer.Size());
        }
#endif

        SV_ULONG cbWritten = 0;
        if( ((cbWritten = tgtvol.Write(pbuffer.Get(), cbread)) != cbread )
            || (!tgtvol.Good()))
        {
            stringstream msg;
            msg << "Error during write operation\n"
                << " Offset : " << tgtvol.Tell()<< '\n'
                << "Expected Write: " << cbread << '\n'
                << "Actual Write Bytes: " << cbWritten << '\n'
                << "Error Code: " << tgtvol.LastError() << '\n'
                << "Error Message: " << Error::Msg(tgtvol.LastError()) << '\n';
            m_err += msg.str();
            m_errno = CDPWriteToTgtFailed;
            break;
        }

        cbRemaining -= cbWritten;
        progress = (SV_ULONG)((tgtvol.Tell() * 100 )/ m_request ->srcVolCapacity);

        if(srcvol.Eof())
        {
            progress = 100;
            cbRemaining =0;
        }

        if(progress && (m_progress != progress))
        {
            m_progress = progress;
            presentTime = ACE_OS::gettimeofday();
            if (presentTime - startTime.sec())
                m_time_remaining_in_secs = (((presentTime - startTime).sec() * 100) / m_progress) - (presentTime - startTime).sec();
            SendProgress();
        }
    }

    if(cbRemaining)
    {
        return false;
    }

    return true;
}

bool SnapshotTask::isBlockUsed(SV_ULONGLONG blocknum,BlockUsageInfo &blockusageinfo )
{
    bool rv = false;
    SV_ULONGLONG relative_block_num = 0;

    //Verify if blocknum passed is with in the range.
    if(blocknum >= blockusageinfo.starting_blocknum && blocknum < (blockusageinfo.block_count + blockusageinfo.starting_blocknum))
    {
        relative_block_num = blocknum - blockusageinfo.starting_blocknum; //Relative block number(index) w.r.t the the bitmap array.
        if(((blockusageinfo.bitmap[relative_block_num / NBITSINBYTE] & (1 << (relative_block_num % NBITSINBYTE))) ? 1 : 0))
        {
            rv = true;
        }
    }
    else
    {
        //Blocknum passed is out of range. returning true
        rv = true;
    }

    return rv;
}

boost::tribool SnapshotTask::copydata_forfsaware_volumes(cdp_volume_t & src, cdp_volume_t &dest, const SV_ULONGLONG & capacity,
                                                         std::string filesystem)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);

    SV_UINT const MAX_RW = m_cdp_max_snapshot_rw_size;
    SV_ULONGLONG cbRemaining = capacity, cbWritten = 0; 
    SV_ULONGLONG curroffset = 0;
    SV_ULONG progress = 0;
    bool rv = true;

    ACE_Time_Value startTime = ACE_OS::gettimeofday(); 
    ACE_Time_Value presentTime = ACE_OS::gettimeofday();
    m_time_remaining_in_secs = 0;

    //The filesystem should be NTFS. Other filesystems are not supported and a full copy of the volume is invoked.
#ifdef SV_WINDOWS
    // enable for REFS when vsnap driver starts supporting ReFS filesystem
    //if(stricmp(filesystem.c_str(),"NTFS") && stricmp(filesystem.c_str(),"REFS"))
    if(stricmp(filesystem.c_str(),"NTFS"))
    {
        return boost::indeterminate;
    }
#else
    return boost::indeterminate;
#endif 

    SharedAlignedBuffer pbuffer(MAX_RW, m_sector_size);
    if( !pbuffer )
    {
        m_err += "Error: insufficient memory to allocate buffers\n";
        m_errno = CDPOUTOFMEMORY;
        rv = false;
        return rv;
    }

    BlockUsageInfo blockusageinfo;
    //Get the used block information for the give volume using the vsnap.
    blockusageinfo.startoffset = 0;
    blockusageinfo.block_count = capacity/m_cdp_max_snapshot_rw_size + ((capacity%m_cdp_max_snapshot_rw_size)?1:0);
    blockusageinfo.block_size = m_cdp_max_snapshot_rw_size;


    if(!get_used_block_information(src.GetName(),dest.GetName(),blockusageinfo))
    {
        stringstream msg;
        msg << "Error during read cluster info for volume" << src.GetName() << "\n";
        m_err += msg.str();
        m_errno = CDPReadFromSrcFailed;
        rv = false;
        return rv;
    }

    // Transfer the contents from source volume to target volume...
    while (!Quit() && cbRemaining ) 
    {
        SV_UINT cbToRead = (SV_UINT) min( cbRemaining, (SV_ULONGLONG)MAX_RW );
        SV_ULONGLONG blocknum = curroffset/m_cdp_max_snapshot_rw_size;

        if(isBlockUsed(blocknum,blockusageinfo))
        {
            src.Seek(curroffset,BasicIo::BioBeg);
            SV_UINT cbread   = src.FullRead(pbuffer.Get(), cbToRead);

            if( (cbread != cbToRead || !src.Good()) && !src.Eof())
            {
                stringstream msg;
                msg << "Error during read operation on volume" << src.GetName() << "\n"
                    << " Offset : " << src.Tell()<< "\n"
                    << "Expected Read Bytes:" << cbToRead << "\n"
                    << "Actual Read Bytes:"   << cbread << "\n"
                    << "Error Code: " << src.LastError() << "\n"
                    << "Error Message: " << Error::Msg(src.LastError()) << "\n";
                m_err += msg.str();
                m_errno = CDPReadFromSrcFailed;
                std::cout<<msg.str();
                break;
            }

#ifdef SV_WINDOWS

            if(0 == curroffset)
            {
                HideBeforeApplyingSyncData(pbuffer.Get(), pbuffer.Size());
            }
#endif

            dest.Seek(curroffset,BasicIo::BioBeg);
            if( ((cbWritten = dest.Write(pbuffer.Get(), cbread)) != cbread )
                || (!dest.Good()))
            {
                stringstream msg;
                msg << "Error during write operation\n"
                    << " Offset : " << dest.Tell()<< '\n'
                    << "Expected Write: " << cbread << '\n'
                    << "Actual Write Bytes: " << cbWritten << '\n'
                    << "Error Code: " << dest.LastError() << '\n'
                    << "Error Message: " << Error::Msg(dest.LastError()) << '\n';
                m_err += msg.str();
                m_errno = CDPWriteToTgtFailed;
                std::cout<<msg.str();
                break;
            }
        }
        else
        {
            cbWritten = cbToRead;
        }

        curroffset += cbWritten;
        cbRemaining -= cbWritten;

        progress = (SV_ULONG)((curroffset * 100 )/capacity );

        if(src.Eof())
        {
            progress = 100;
            cbRemaining =0;
        }

        if(progress && (m_progress != progress))
        {
            m_progress = progress;
            presentTime = ACE_OS::gettimeofday();
            if (presentTime - startTime.sec())
                m_time_remaining_in_secs = (((presentTime - startTime).sec() * 100) / m_progress) - (presentTime - startTime).sec();
            SendProgress();
        }
    }

    if(cbRemaining)
    {
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);

    return rv;
}

/*
* FUNCTION NAME :  SnapshotTask::removevsnap
*
* DESCRIPTION : remove vsnap corresponding to target volume
*
* INPUT PARAMETERS : target volume name
*                    
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::removevsnap(const std::string & volumename)
{
    VsnapMgr *Vsnap;
#ifdef SV_WINDOWS
    WinVsnapMgr obj;
#else
    UnixVsnapMgr obj;
#endif

    Vsnap=&obj;
    std::string formattedName = volumename;
    std::string sparsefile;
    bool newvirtualvolume = false; 
    bool is_volpack = IsVolPackDevice(volumename.c_str(),sparsefile,newvirtualvolume);

#ifdef SV_WINDOWS
    if(is_volpack)
    {
        Vsnap->UnMountVsnapVolumesForTargetVolume(formattedName,true);
    }
    else if ( IsDrive(formattedName))
    {
        UnformatVolumeNameToDriveLetter(formattedName);
        formattedName += ":";
        Vsnap->UnMountVsnapVolumesForTargetVolume(formattedName,true);    
    }
    else if(IsMountPoint(formattedName))
    {                   
        FormatVolumeNameForMountPoint(formattedName);
        // TODO : Need to check target volume  C:\mnt\ or C:\mnt.
        // Currently using C:\mnt
        formattedName.erase(formattedName.length() - 1, 1);            
        Vsnap->UnMountVsnapVolumesForTargetVolume(formattedName,true);           
    }            
#else
    Vsnap->UnMountVsnapVolumesForTargetVolume(formattedName,true);        
#endif

    return true;
}

/*
* FUNCTION NAME :  SnapshotTask::rollback
*
* DESCRIPTION : rollback target volume using specified db/rollback pt
*
* INPUT PARAMETERS : target volume name
*                    
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
* return value : true on success, otherwise false
*
*/
bool SnapshotTask::rollback(CDPDatabase &db, 
                            CDPDRTDsMatchingCndn & cndn,
                            cdp_volume_t & tgtvol, 
                            const SV_ULONGLONG & tsfc,
                            const SV_ULONGLONG & tslc)
{

    bool done = false;
    SV_ULONG progress = 0;
    SV_ULONGLONG recovery_period = 0;
    SV_ULONGLONG recovered_period = 0;
    SVERROR sv = SVS_OK;
    bool failure = false;
    SV_UINT totalBytesRead = 0;
    SV_UINT cbRemaining = 0;
    SV_UINT cbWritten = 0;
    SV_UINT cbRead = 0;
    SV_UINT cbWrite = 0;
    SV_UINT readin = 0;
    std::string filename;
    std::string pathname;
    BasicIo retentionFile;
    cdp_rollback_drtd_t drtd;
    cdp_io_stats_t stats;
    std::stringstream rollback_starttime;
    std::stringstream rollback_endtime;

    bool simulatesparse = m_localConfigurator.SimulateSparse();

    ACE_Time_Value startTime = ACE_OS::gettimeofday(); 
    ACE_Time_Value presentTime = ACE_OS::gettimeofday();
    m_time_remaining_in_secs = 0;

    recovery_period = tslc - cndn.toTime();

    ACE_TCHAR timeStamp[64] = { 0 };
    if (ACE::timestamp(timeStamp, NELEMS(timeStamp)))
    {
        rollback_starttime << ACE_TEXT_ALWAYS_CHAR(timeStamp);
    }
        
    // 
    // allocate memory
    //
    SV_UINT const MAX_RW = m_cdp_max_snapshot_rw_size;

    SharedAlignedBuffer pbuffer(MAX_RW, m_sector_size);
    if( !pbuffer )
    {
        m_err += "Error: insufficient memory to allocate buffers\n";
        m_errno = CDPOUTOFMEMORY;
        return false;
    }

    if(!rollbackPartiallyAppliedChanges(db, tgtvol))
    {
        m_err += "Error: read failure from retention store \n";
        m_errno = CDPReadFromRetentionFailed;
        return false;
    }

    m_progress = 0;
    CDPDatabaseImpl::Ptr dbptr = db.FetchDRTDs(cndn);

    if(!dbptr)
    {
        DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s initialize db pointer failed.\n",
            FUNCTION_NAME);
        return false;
    }

    Initialize_RollbackStats(); // Initialize Rollbackstats Structure Members
        
    while ((sv = dbptr -> read(drtd)) == SVS_OK ) 
    {
        if(Quit())
        {
            break;
        }

        if(drtd.filename.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s recieved empty filename.\n",
                FUNCTION_NAME);
            failure = true;
            m_err += "Error: read failure from retention store\n";
            m_errno = CDPReadFromRetentionFailed;
            break;
        }

        ACE_stat statbuf;
        std::string tmppathname = db.dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A;
        tmppathname += drtd.filename;

        if(simulatesparse)
        {
            if(0 != sv_stat(getLongPathName(tmppathname.c_str()).c_str(),&statbuf))
            {
                DebugPrintf(SV_LOG_DEBUG,"%s is not available (coalesced file?)\n", drtd.filename.c_str());
                continue;
            } 
        }


        if(filename != drtd.filename)
        {
            retentionFile.Close();

            filename = drtd.filename;
            pathname = db.dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A;
            pathname += filename;

            retentionFile.Open(pathname, BasicIo::BioOpen |BasicIo::BioRead |  BasicIo::BioShareRW | BasicIo::BioBinary);
            if(!retentionFile.Good())
            {
                failure = true;
                m_err += "Error: read failure from retention store\n";
                m_errno = CDPReadFromRetentionFailed;
                break;
            }
        }

        cbRemaining = drtd.length;
        totalBytesRead = 0;
        cbWritten = 0;
        cbRead = 0;
        cbWrite = 0;

        while( cbRemaining > 0)
        {
            cbRead = min(cbRemaining, MAX_RW);

            retentionFile.Seek( (drtd.fileoffset+ totalBytesRead), BasicIo::BioBeg);
            readin = retentionFile.FullRead(pbuffer.Get(),cbRead);

            if( readin != cbRead )
            {
                stringstream l_stdfatal;
                l_stdfatal   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error during read operation on differential file:" << retentionFile.GetName() << "\n"
                    << " Offset : " << ( drtd.fileoffset+ totalBytesRead ) << "\n"
                    << "Expected Read:" << cbRead << "\n"
                    << "Actual Read:"   << readin << "\n"
                    << " Error Code: " << retentionFile.LastError() << "\n"
                    << " Error Message: " << Error::Msg(retentionFile.LastError()) << "\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                m_err += "Error: read failure from retention store\n";
                m_errno = CDPReadFromRetentionFailed;
                failure = true;
                break;
            }
            m_rollbackstats.retentionfile_read_bytes += readin; // read bytes
            ++m_rollbackstats.retentionfile_read_io; // read_Ios
            
            tgtvol.Seek(drtd.voloffset + cbWritten, BasicIo::BioBeg);
            if(!tgtvol.Good())
            {
                m_err += "Error: seek failed on target volume\n";
                m_errno = CDPSeekOnTgtFailed;
                failure = true;
                break;
            }

            if( ((cbWrite = tgtvol.Write(pbuffer.Get(), cbRead)) != cbRead )
                || (!tgtvol.Good()))
            {
                stringstream msg;
                msg << "Error during write operation\n"
                    << " Offset : " << tgtvol.Tell()<< '\n'
                    << "Expected Write: " << cbRead << '\n'
                    << "Actual Write Bytes: " << cbWrite << '\n'
                    << "Error Code: " << tgtvol.LastError() << '\n'
                    << "Error Message: " << Error::Msg(tgtvol.LastError()) << '\n';
                m_err += msg.str();
                m_errno = CDPWriteToTgtFailed;
                failure = true;
                break;
            }

            totalBytesRead += cbRead;
            cbWritten += cbRead;
            m_rollbackstats.tgtvol_write_bytes += cbWrite; // write bytes
            ++m_rollbackstats.tgtvol_write_io;  // write_Ios
            cbRemaining -= cbRead;

        } // end of while loop transferring one dirty block

        // if we could not complete the transfer,
        // there was an error and we will break further transfer
        if(failure)
        {
            break;
        }

        recovered_period = tslc - drtd.timestamp;
        if(recovery_period)
            progress = (SV_ULONG)(recovered_period * 100 / recovery_period);
        else
            progress = 100;

        if(progress && (m_progress != progress))
        {
            m_progress = progress;
            presentTime = ACE_OS::gettimeofday();
            if (presentTime - startTime.sec())
                m_time_remaining_in_secs = (((presentTime - startTime).sec() * 100) / m_progress) - (presentTime - startTime).sec();
            if(0 == m_progress % 5)
                SendProgress();
        }    

    } // end while loop iterating through the drtds

    if(sv.failed())
    {
        DebugPrintf(SV_LOG_ERROR,"FUNCTION:%s read from db failed.\n",
            FUNCTION_NAME);
        m_err += "Error: read failure from retention store\n";
        m_errno = CDPReadFromRetentionFailed;

    } else if((sv == SVS_FALSE) && !Quit() && !failure)
    {
        progress = 100;
        m_progress = progress;
        m_time_remaining_in_secs = 0;
        SendProgress();
        done = true;
    }

    //Rollback Operation Start time
    m_rollbackstats.rollback_operation_starttime = rollback_starttime.str();

    //Rollback Operation End time
    if (ACE::timestamp(timeStamp, NELEMS(timeStamp)))
    {
        rollback_endtime << ACE_TEXT_ALWAYS_CHAR(timeStamp);
    }
    m_rollbackstats.rollback_operation_endtime = rollback_endtime.str();

    //get rollback metadata stats
    stats = dbptr -> get_io_stats();
    m_rollbackstats.metadatafile_read_io = stats.read_io_count;
    m_rollbackstats.metadatafile_read_bytes = stats.read_bytes_count;

    
    //First data change timestamp
    m_rollbackstats.firstchange_timeStamp = tsfc;

    //Latest data change timestamp
    m_rollbackstats.lastchange_timeStamp = tslc;

    //The selected recovery period is from latest data change
    m_rollbackstats.Recovery_Period = recovery_period;
    
    return done;
}

bool SnapshotTask::rollbackPartiallyAppliedChanges(CDPDatabase & db, cdp_volume_t & tgtvol)
{
    bool rv = true;

    do
    {
        SVERROR sv = SVS_OK;
        cdp_rollback_drtd_t drtd;
        std::string cdpDataFile;
        BasicIo dataFileBio;
        CDPDatabaseImpl::Ptr dbptr = db.FetchAppliedDRTDs();

        if(!dbptr)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s initialize db pointer failed.\n",
                FUNCTION_NAME);
            rv = false;
            break;
        }

        while((sv = dbptr ->read(drtd)) == SVS_OK)
        {

            if(drtd.filename.empty())
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s recieved empty filename.\n",
                    FUNCTION_NAME);
                rv = false;
                break;
            }

            if(cdpDataFile != drtd.filename)
            {
                dataFileBio.Close();

                cdpDataFile = drtd.filename;
                std::string filePath = db.dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A;
                filePath += cdpDataFile;

                dataFileBio.Open(filePath, BasicIo::BioReadExisting | BasicIo::BioShareRead |
                    BasicIo::BioShareWrite | BasicIo::BioShareDelete);
                if(!dataFileBio.Good())
                {
                    DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s open %s failed. error no:%d\n",
                        FUNCTION_NAME, filePath.c_str(), ACE_OS::last_error());
                    rv = false;
                    break;
                }
            }

            SharedAlignedBuffer pbuffer(drtd.length, m_sector_size);
            if( !pbuffer )
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s insufficient memory to allocate buffers\n",
                    FUNCTION_NAME);
                rv = false;
                break;
            }

            if(!readDRTDFromDataFile(dataFileBio, pbuffer.Get(), drtd))
            {
                rv = false;
                break;
            }

            if(!write_sync_drtd_to_volume(tgtvol, pbuffer.Get(), drtd))
            {
                rv = false;
                break;
            }
        }

        if(sv.failed())
        {
            DebugPrintf(SV_LOG_ERROR,"FUNCTION:%s read from db failed.\n",
                FUNCTION_NAME);
            rv = false;
            break;
        }

    }while(false);

    return rv;
}

bool SnapshotTask::readDRTDFromDataFile(BasicIo &bIo, char *buffer, const cdp_rollback_drtd_t &drtd)
{
    bool rv = true;
    SV_UINT const MAX_RW = m_max_rw_size ;
    SV_OFFSET_TYPE drtdOffset = drtd.fileoffset;
    SV_UINT drtdRemainingLength = drtd.length;

    do
    {
        if(bIo.Seek(drtdOffset, BasicIo::BioBeg) != drtdOffset)
        {
            stringstream l_stderr;
            l_stderr   << "Seek to offset " << drtdOffset
                << " failed for  " << bIo.GetName()
                << ". error code: " << ACE_OS::last_error() << std::endl;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        while(drtdRemainingLength > 0)
        {
            SV_UINT bytesToRead = min(MAX_RW, drtdRemainingLength);
            SV_UINT bytesRead = 0;

            if( (bytesRead = bIo.Read(buffer, bytesToRead)) != bytesToRead)
            {
                stringstream l_stderr;
                l_stderr   << "Read at offset " << drtdOffset
                    << " failed for  " << bIo.GetName()
                    << ". error code: " << ACE_OS::last_error() << std::endl;

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }
            drtdRemainingLength -= bytesRead;
            drtdOffset += bytesRead;
            buffer += bytesRead;
        }
    }while(false);
    return rv;
}

bool SnapshotTask::write_sync_drtd_to_volume(cdp_volume_t & tgtvol, char *buffer, const cdp_rollback_drtd_t &drtd)
{
    bool rv = true;
    SV_UINT const MAX_RW = m_max_rw_size ;

    SV_OFFSET_TYPE volumeOffset = drtd.voloffset;
    SV_UINT drtdRemainingLength = drtd.length;
    do
    {
        if(tgtvol.Seek(volumeOffset, BasicIo::BioBeg) != volumeOffset)
        {
            stringstream l_stderr;
            l_stderr   << "Seek to offset " << volumeOffset
                << " failed for volume " << tgtvol.GetName()
                << ". error code: " << ACE_OS::last_error() << std::endl;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        while(drtdRemainingLength > 0)
        {
            SV_UINT bytesToWrite = min(MAX_RW, drtdRemainingLength);
            SV_UINT bytesWritten = 0;

            if( (bytesWritten = tgtvol.Write(buffer, bytesToWrite)) != bytesToWrite)
            {
                stringstream l_stderr;
                l_stderr   << "Write at offset " << volumeOffset
                    << " failed for volume " << tgtvol.GetName()
                    << ". error code: " << ACE_OS::last_error() << std::endl;

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }
            drtdRemainingLength -= bytesWritten;
            volumeOffset += bytesWritten;
            buffer += bytesWritten;
        }
    }while(false);
    return rv;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int SnapshotTask::open(void *args)
{
    return this->activate(THR_BOUND);
}    

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int SnapshotTask::close(u_long flags)
{
    // drain message queue and free messages?
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// does the actual work of taking the snapshot as per the request
// --------------------------------------------------------------------------
int SnapshotTask::svc()
{
    try
    {
        //__asm int 3;
        InitializeConfigurator();
        m_cdp_max_snapshot_rw_size = m_localConfigurator.getCdpMaxSnapshotIOSize();
        m_max_rw_size  = m_localConfigurator.getCdpMaxIOSize();
        m_sector_size = m_localConfigurator.getVxAlignmentSize();
        m_directio = m_localConfigurator.useUnBufferedIo();

        // solaris : always buffering i.e m_directio=false
        // aix: always o_direct i.e m_directio=true
#ifdef SV_SUN
        m_directio=false;
#endif

#ifdef SV_AIX
        m_directio=true;
#endif

        switch(m_request -> operation)
        {
        case SNAPSHOT_REQUEST::PLAIN_SNAPSHOT:
            snapshot();
            break;

        case SNAPSHOT_REQUEST::PIT_VSNAP:
            vsnap_mount();
            break;

        case SNAPSHOT_REQUEST::RECOVERY_VSNAP:
            vsnap_mount();
            break;

        case SNAPSHOT_REQUEST::VSNAP_UNMOUNT:
            vsnap_unmount();
            break;

        case SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT:
            recover();
            break;

        case SNAPSHOT_REQUEST::ROLLBACK:
            rollback();
            break;

        default:
            break;
        }

    } catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR,"\n%s encountered exception %s.\n",FUNCTION_NAME, ce.what());
        failImmediate();
        CDPUtil::SignalQuit();
    }
    catch (std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, "%s caught exception %s\n", FUNCTION_NAME, e.what());
        failImmediate();
        CDPUtil::SignalQuit();
    } catch(...) {
        DebugPrintf(SV_LOG_ERROR, 
            "%s caught an unknown exception\n", FUNCTION_NAME );
        failImmediate();
        CDPUtil::SignalQuit();
    }

    return 0;
} 

// --------------------------------------------------------------------------
// posts a message to the message queue
// --------------------------------------------------------------------------
void SnapshotTask::PostMsg(int msgId, int priority)
{
    ACE_Message_Block *mb = new ACE_Message_Block(0, msgId);
    mb->msg_priority(priority);
    msg_queue()->enqueue_prio(mb);
}

// --------------------------------------------------------------------------
// checks if a quit message has been sent.
// --------------------------------------------------------------------------
bool SnapshotTask::Quit(int waitSeconds)
{
    ACE_Message_Block *mb;
    ACE_Time_Value waitTime;

    if (m_quit)
        return true;

    if(waitSeconds)
    {
        ACE_Time_Value waitfor(ACE_OS::gettimeofday());
        waitTime.sec(waitfor.sec() + waitSeconds);
    }

    if (-1 == this->msg_queue()->dequeue_head(mb, &waitTime)) {
        if (errno == EWOULDBLOCK || errno == ESHUTDOWN) {
            return false;
        }
        return true;
    }

    if (CDP_QUIT == mb->msg_type())
    {
        m_quit = true;
        m_err += "aborted due to service shutdown request.\n";
        m_errno = CDPServiceQuit;
    }        
    else if(CDP_ABORT == mb->msg_type())
    {
        m_quit = true;
        m_err += "aborted by user.\n";
        m_errno = CDPAbortedbyUser;
    }

    mb->release();
    return m_quit;
}

bool SnapshotTask::snapshot()
{
    do
    {
        try
        {
            // 
            // we move to start state
            //
            m_state = SNAPSHOT_REQUEST::SnapshotStarted;
            SendStateChange();

            //
            // checks on snapshot source
            // 1) should exist
            // 2) should be hidden volume
            // 3) should be replicated volume in case of recovery/rollback (and vsnap)
            // 4) should not be in resync for physical snapshot (and pit vsnap)
            std::string sparsefile;
            bool newvirtualvolume = false; 
            bool is_volpack = IsVolPackDevice(m_request ->src.c_str(),sparsefile,newvirtualvolume);
            //skipping the existance of file existance in case of multisparse file
            //as it is taken care inside the function IsVolPackDevice
            //skipping the isvolumelocked check as we donot have to do anything for multisparse
            if(!newvirtualvolume)
            {
                if(!IsFileORVolumeExisting(m_request ->src.c_str()))
                {
                    m_err += "source volume does not exist.\n";
                    m_state = SNAPSHOT_REQUEST::Failed;
                    SendStateChange();
                    break;
                }

                // The caller should ensure that the source volume is locked (hidden) 
                // and remains so until the snapshot completes
                if(!m_skipCheck && !IsVolumeLocked(m_request ->src.c_str()))
                {
                    m_err += "snapshot cannot be taken from ";
                    m_err += m_request ->src;
                    m_err += ". It is in unlocked state.\n";
                    m_state = SNAPSHOT_REQUEST::Failed;
                    SendStateChange();
                    break;
                }
            }

            if(!m_skipCheck && isResyncing(m_request ->src)) 
            {
                m_err += "snapshot cannot be taken from " ;
                m_err += m_request ->src;
                m_err += ". It is a undergoing resync.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            //checks on snapshot destination mountpoint
            // the mount point should not be "/", "/usr", "/home", "/tmp",
            //"/var", "/etc", "/var/crash", "/boot", "/sys", any directory under "/proc", 
            //any directory under "/dev" are not allowed,
            //any parent directory which can cause Vx Cache dir, Vx install dir
            std::string errmsg;
            if(!IsValidMountPoint(m_request ->destMountPoint,errmsg))
            {
                m_err += "snapshot cannot be taken on ";
                m_err += m_request->dest;
                m_err += ". ";
                m_err += errmsg;
                m_err += "\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            //
            // checks on snapshot destination
            // 1) should exist
            // 2) should not be protected volume
            // 3) should not be replicated volume
            // 4) should not contain retention files
            // 5) should not contain vsnap map files
            // 6) should not contain volpack data files
            // 7) should not be readonly
            // 8) should not contain mounted volumes
            // 9) should not contain partitions

            if(!IsFileORVolumeExisting(m_request ->dest.c_str()))
            {
                m_err += "destination volume does not exist.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }
            //Bug #5685
            //Checking if the snapshot dest is valid device
            //This is needed in case mountpoint is provided from cdpcli

            if(!IsValidDevfile(m_request->dest))
            {
                m_err += "destination volume isn't a valid device.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_srcVolRollback && !m_skipCheck && isProtectedVolume(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It is a protected volume.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && isTargetVolume(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It is a target volume.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && containsRetentionFiles(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains retention data.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && containsVsnapFiles(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains vsnap data.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && containsVolpackFiles(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains virtual volume data.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            bool readonly = false;
            if(isReadOnly(m_request -> dest.c_str(), readonly).failed() || readonly)
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It is a read-only volume.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            std::string mountedvolname;
            if(!m_skipCheck && containsMountedVolumes(m_request -> dest, mountedvolname))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains mounted volumes";
                m_err += "(";
                m_err += mountedvolname ;
                m_err += ")\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            std::string errormsg;
            if(!m_skipCheck && !IsValidDestination(m_request -> dest, errormsg))
            {
                m_err += errormsg;
                DebugPrintf(SV_LOG_ERROR, "Invalid destination specified : %s\n", errormsg.c_str());
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            // 
            // common checks
            // 1) source and target have to be different
            // 2) target capacity should be greater than source

            if(sameVolumeCheck(m_request ->src, m_request -> dest))
            {
                m_err += "snapshot source and destination cannot be same.\n" ;

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck &&!computeSrcCapacity(m_request -> src, m_request -> srcVolCapacity))
            {
                m_err += "source volume: ";
                m_err += m_request ->src ;
                m_err += " capacity could not be computed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!computDestCapacity(m_request -> dest, m_destCapacity))
            {
                m_err += "Destination volume: ";
                m_err += m_request ->dest ;
                m_err += " capacity could not be computed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if( m_destCapacity < m_request -> srcVolCapacity)
            {
                m_err += "Target Volume capacity is less than Source Volume\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            CDPLock recoverCdpLock(m_request -> src);
            if(!recoverCdpLock.acquire_read())
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += " due to aquaring read lock on the volume ";
                m_err += m_request -> src;
                m_err += " failed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            CDPLock destCdplock(m_request ->dest);
            if(!destCdplock.acquire())
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += " due to aquaring write lock on the volume ";
                m_err += m_request ->dest;
                m_err += " failed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            // run pre-script
            if(!runprescript())
            {
                m_errno = CDPPreScriptFailed;
                failImmediate();
                break;
            }

            // move to copy in progress state
            m_state = SNAPSHOT_REQUEST::SnapshotInProgress;
            SendStateChange();

            cdp_volume_t srcvol(m_request ->src,newvirtualvolume);


            // 
            // Enable Retries on failures (required for fabric multipath device failures)
            //
            srcvol.EnableRetry(CDPUtil::QuitRequested,GetRetries(), GetRetryDelay());

            // 
            // get read access to source volume
            //
            BasicIo::BioOpenMode  srcvol_mode= (BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioSequentialAccess);
            if(m_directio)
            {
#ifdef SV_WINDOWS
                srcvol_mode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
                srcvol_mode |= BasicIo::BioDirect;
#endif
            }
            srcvol.Open(srcvol_mode);
            if (!srcvol.Good()) 
            {
                stringstream msg;
                msg << "failed to open source volume :" 
                    << srcvol.GetName()<< " error: " << srcvol.LastError() << '\n';
                m_err += msg.str();
                m_errno = CDPSrcOpenFailed;
                fail();
                break;
            }

            cdp_volume_t tgtvol(m_request ->dest);

            //
            // Enable Retries on failures (required for fabric multipath device failures)
            //
            tgtvol.EnableRetry(CDPUtil::QuitRequested,GetRetries(), GetRetryDelay());

            // 
            // get exclusive access (hide and open write mode) to target volume
            //
            if(!tgtvol.OpenExclusive() || !tgtvol.Good())
            {
                stringstream msg;
                msg << "failed to open target volume :" 
                    << tgtvol.GetName()<< " error: " << tgtvol.LastError() << '\n';
                m_err += msg.str();
                m_errno = CDPTgtOpenFailed;
                fail();
                break;
            }

            std::string filesystem = getFSType(m_request ->src);
            boost::tribool res = false;

            if(m_use_fsawarecopy)
            {
                res = copydata_forfsaware_volumes(srcvol,tgtvol,  m_request ->srcVolCapacity, filesystem);
            }

            //If fsawarecopy is not enabled or fsaware copy is not supported or failed, invoke the normal copydata.
            if((!m_use_fsawarecopy || FsAwareCopyNotSupported(res) || !res))
            {
                if(!copydata(srcvol,tgtvol,  m_request ->srcVolCapacity))
                {
                    srcvol.Close();
                    tgtvol.Close();
                    fail();
                    break;
                }
            }

#ifdef SV_SUN

            // On sun, we are opening in buffered mode. so we need to flush
            if(ACE_INVALID_HANDLE != tgtvol.GetHandle())
            {
                if(ACE_OS::fsync(tgtvol.GetHandle()) == -1)
                {
                    stringstream msg;
                    msg << "failed in flush writes to target volume :" 
                        << tgtvol.GetName()<< " error: " << ACE_OS::last_error() << '\n';
                    m_err += msg.str();
                    m_errno = CDPWriteToTgtFailed;
                    srcvol.Close();
                    tgtvol.Close();
                    fail();
                    break;
                }
            }
#endif

            srcvol.Close();
            tgtvol.Close();

            // 
            // unhide the destination volume
            //
            string fsType = getFSType(m_request ->src);
            if(!makeVolumeAvailable(m_request ->dest,m_request->destMountPoint,fsType))
            {
                fail();
                break;
            }

            // run postscript
            if(!runpostscript())
            {
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            m_state = SNAPSHOT_REQUEST::Complete;
            SendStateChange();
        }catch ( ContextualException& ce )
        {
            std::string errormsg = "snapshot cannot be taken. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered exception ";
            errormsg += ce.what();
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }
        catch( exception const& e )
        {
            std::string errormsg = "snapshot cannot be taken. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered exception ";
            errormsg += e.what();
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }
        catch ( ... )
        {
            std::string errormsg = "snapshot cannot be taken. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered unknown exception.";
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }

    } while (0);

    return true;
}
bool SnapshotTask::rollback()
{

    bool breakReplication = false;
    bool bresyncRequired  = false;
    CDPLock purge_lock(m_request -> src, true, ".purgelock");
    CDPLock srcCdpLock(m_request -> src);

    do
    {
        try
        {
            //__asm int 3;

            // 
            // we move to start state
            //
            m_state = SNAPSHOT_REQUEST::RollbackStarted;
            SendStateChange();

            if (!SetVolumesCache())
            {
                m_err += "volume information cache does not exist.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }


            std::string displayName;
            cdp_volume_t::CDP_VOLUME_TYPE volumeType;

            GetDeviceProperties(m_request->dest, displayName, volumeType);

            // 
            //
            // checks on rollback volume
            // 1) should exist
            // 2) should be hidden volume
            // 3) should be replicated volume in case of recovery/rollback (and vsnap)
            // 4) should not be in resync for physical snapshot (and pit vsnap)
            std::string sparsefile;
            bool newvirtualvolume = false; 
            bool is_volpack = IsVolPackDevice(m_request ->dest.c_str(),sparsefile,newvirtualvolume);


            cdp_volume_t tgtvol(m_request->dest, newvirtualvolume, volumeType, &m_VolumesCache.m_Vs);

            //skipping the existance of file existance in case of multisparse file
            //as it is taken care inside the function IsVolPackDevice
            //skipping the isvolumelocked check as we donot have to do anything for multisparse

            if(!newvirtualvolume)
            {
                if(!IsFileORVolumeExisting(displayName.c_str()))
                {
                    m_err += "rollback volume does not exist.\n";
                    m_state = SNAPSHOT_REQUEST::Failed;
                    SendStateChange();
                    break;
                }

                // The caller should ensure that the source volume is locked (hidden) 
                // and remains so until the snapshot completes
                if(!m_skipCheck && !tgtvol.IsVolumeLocked())
                {
                    m_err += "rollback cannot be performed on ";
                    m_err += m_request ->src;
                    m_err += ". It is in unlocked state.\n";
                    m_state = SNAPSHOT_REQUEST::Failed;
                    SendStateChange();
                    break;
                }
            }
            //BugId:6701 skip this checking for source volume
            if(!m_srcVolRollback && !m_skipCheck)
            {
                if((!isTargetVolume(m_request ->dest)))
                {
                    m_err += "rollback cannot be perform on " ;
                    m_err += m_request ->dest;
                    m_err += "It is not a target volume.\n";

                    m_state = SNAPSHOT_REQUEST::Failed;
                    SendStateChange();
                    break;
                }
            }

            //checks on snapshot destination mountpoint
            // the mount point should not be "/", "/usr", "/home", "/tmp",
            //"/var", "/etc", "/var/crash", "/boot", "/sys", any directory under "/proc", 
            //any directory under "/dev" are not allowed,
            //any parent directory which can cause Vx Cache dir, Vx install dir
            std::string errmsg;
            if(!IsValidMountPoint(m_request ->destMountPoint,errmsg))
            {
                m_err += "rollback cannot be performed on ";
                m_err += m_request->dest;
                m_err += ". ";
                m_err += errmsg;
                m_err += "\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            // while purge operation is in progress, we do not want
            // any recovery jobs to access the retention files
            purge_lock.acquire();

            if(!srcCdpLock.acquire())
            {
                m_err += "rollback cannot be performed on ";
                m_err += m_request ->src;
                m_err += " due to acquiring write lock on the volume failed.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }
            //
            // verify rollback point
            //

            SV_TIME svtime;
            SV_ULONGLONG recoverypt;
            sscanf(m_request ->recoverypt.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                &svtime.wYear,
                &svtime.wMonth,
                &svtime.wDay,
                &svtime.wHour,
                &svtime.wMinute,
                &svtime.wSecond,
                &svtime.wMilliseconds,
                &svtime.wMicroseconds,
                &svtime.wHundrecNanoseconds);
            if(!CDPUtil::ToFileTime(svtime, recoverypt))
            {
                m_err += "specified rollback point is invalid\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            bool isconsistent = false;
            bool isavailable = false;

            std::string displaytime;
            CDPUtil::CXInputTimeToConsoleTime(m_request ->recoverypt, displaytime);

            if( !CDPUtil::isCCP(m_request->dbpath,recoverypt,isconsistent, isavailable)|| !isavailable )
            {
                std::stringstream errstring;
                errstring << " The specified time " << displaytime << " is not a valid recovery point.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                m_err += errstring.str();
                SendStateChange();
                break;
            }

            if(!isconsistent)
            {
                std::stringstream infostring;
                infostring << " The specified time " << displaytime << " is not a crash consistent point.\n";
                m_state = SNAPSHOT_REQUEST::RollbackStarted;
                m_err += infostring.str();
                SendStateChange();
            }

            // 
            // run pre script
            //
            if(!runprescript())
            {
                m_errno = CDPPreScriptFailed;
                failImmediate();
                break;
            }

            // move to copy in progress state
            m_state = SNAPSHOT_REQUEST::RollbackInProgress;
            SendStateChange();

            //
            // get database handle
            //
            CDPDatabase database(m_request->dbpath);
            CDPSummary summary;
            if(!database.getcdpsummary(summary))
            {
                m_err += "Error: read failure from retention store \n";
                m_errno = CDPReadFromRetentionFailed;
                fail();
                break;
            }

            //
            // verify rollback point is within retention window range
            //

            string datefrom, dateto, recoveryto;
            if(!CDPUtil::ToDisplayTimeOnConsole(summary.start_ts, datefrom))
            {
                m_err += "Error: read failure from retention store \n";
                m_errno = CDPReadFromRetentionFailed;
                fail();
                break;
            }

            if(!CDPUtil::ToDisplayTimeOnConsole(summary.end_ts, dateto))
            {
                m_err += "Error: read failure from retention store \n";
                m_errno = CDPReadFromRetentionFailed;
                fail();
                break;
            }

            if(!CDPUtil::ToDisplayTimeOnConsole(recoverypt, recoveryto))
            {
                m_err += "Error: specified time is invalid\n";
                m_errno = CDPReadFromRetentionFailed;

            }

            if(recoverypt < summary.start_ts || recoverypt > summary.end_ts)
            {
                m_err += "The specified time: ";
                m_err += recoveryto;
                m_err += " is not within available recovery time range (";
                m_err += datefrom;
                m_err += " to ";
                m_err += dateto;
                m_err += ").\n";
                fail();
                break;
            } 

            CDPDRTDsMatchingCndn cndn;
            cndn.toEvent(m_request ->recoverytag); 
            cndn.toTime(recoverypt);
            cndn.toSequenceId(m_request->sequenceId);

            //
            // remove vsnaps for the target volume before applying rollback data
            //
            //BugId:6701 skip removal of vsnaps for source volume
            if(!m_srcVolRollback && !m_skipCheck)
            {
                removevsnap(m_request->dest);
            }

            // 
            // get exclusive access (hide and open readwrite) to target volume
            //


            // Enable Retries on failures (required for fabric multipath device failures)
            tgtvol.EnableRetry(CDPUtil::QuitRequested,GetRetries(), GetRetryDelay());

            if(!tgtvol.OpenExclusive() || !tgtvol.Good())
            {
                stringstream msg;
                msg << "failed to open target volume :" 
                    << tgtvol.GetName()<< " error: " << tgtvol.LastError() << '\n';
                m_err += msg.str();
                m_errno = CDPTgtOpenFailed;
                fail();
                break;
            }


            // 
            // transfer contents from retention log to the target volume ...
            //

            if(!rollback(database, cndn, tgtvol, summary.start_ts, summary.end_ts))
            {
                tgtvol.Close();
                fail();
                bresyncRequired = true;
                break;
            }

#ifdef SV_SUN

            // On sun, we are opening in buffered mode. so we need to flush
            if(ACE_INVALID_HANDLE != tgtvol.GetHandle())
            {
                if(ACE_OS::fsync(tgtvol.GetHandle()) == -1)
                {
                    stringstream msg;
                    msg << "failed in flush writes to target volume :" 
                        << tgtvol.GetName()<< " error: " << ACE_OS::last_error() << '\n';
                    m_err += msg.str();
                    m_errno = CDPWriteToTgtFailed;
                    tgtvol.Close();
                    fail();
                    break;
                }
            }
#endif

            tgtvol.Close();

            // 
            // unhide the destination volume
            //
            if (cdp_volume_t::CDP_DISK_TYPE != volumeType){
                if (m_localConfigurator.IsVolpackDriverAvailable() || !is_volpack)
                {
                    string fsType = getFSType(m_request->dest);
                    if (!makeVolumeAvailable(m_request->dest, m_request->destMountPoint, fsType))
                    {
                        fail();
                        bresyncRequired = true;
                        break;
                    }

                }
            }

            //BugId:6701 skip break replication for source volume
            if(!m_srcVolRollback && !m_skipCheck)
            {
                breakReplication = true;
            }

            // run postscript
            if(!runpostscript())
            {
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            m_state = SNAPSHOT_REQUEST::Complete;
            SendStateChange();
        } catch ( ContextualException& ce )
        {
            std::string errormsg = "rollback cannot be performed. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered exception ";
            errormsg += ce.what();
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }
        catch( exception const& e )
        {
            std::string errormsg = "rollback cannot be performed. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered exception ";
            errormsg += e.what();
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }
        catch ( ... )
        {
            std::string errormsg = "rollback cannot be performed. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered unknown exception.";
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }
    } while (0);
    string cleanupaction;
    if(breakReplication) 
    {
        std::string cxFormattedVolumeName = m_request -> dest;
        FormatVolumeNameForCxReporting(cxFormattedVolumeName);
        if(m_stopReplicationAtCX)
        {
            cleanupaction = "cleanupaction=";
            //doing clean up action
            string error;
            if(!CDPUtil::CleanCache(m_request->dest,error))
                DebugPrintf(SV_LOG_INFO,"%s\n",error.c_str());
            error.clear();
            if(CDPUtil::CleanPendingActionFiles(m_request->dest,error).failed())
                DebugPrintf(SV_LOG_INFO,"%s\n",error.c_str());
            error.clear();
            // PR#10815: Long Path support
            std::vector<char> vdb_dir(SV_MAX_PATH, '\0');
            DirName(m_request->dbpath.c_str(),&vdb_dir[0], vdb_dir.size());
            if(m_request->cleanupRetention)
            {
                cleanupaction += "yes;deleteretentionlog=yes;";
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Manualy delete the retention db and files in path %s if retention log is not needed.\n", vdb_dir.data());
                cleanupaction += ";";
            }

            // Removes the replication pair after rollback is done    
            Configurator* TheConfigurator = 0;
            if(!GetConfigurator(&TheConfigurator))
            {
                DebugPrintf(SV_LOG_DEBUG, "Unable to obtain configurator for %s %s %d\n", cxFormattedVolumeName.c_str(), FUNCTION_NAME, LINE_NO);
            }
            else
            {                    
                if(cdpStopReplication(*TheConfigurator,cxFormattedVolumeName,cleanupaction))
                {
                    cleanupaction.clear();
                    cleanupaction = "sent_stop_replication_to_cx";
                }
            }


            // Create a file under the application
            // install directory and write the content into the file if the deletion request failed 
            // it will write the clean up message otherwise write "sent_stop_replication_to_cx" .
            // on detection of this file dataprotection will not be launched for this rollback volume 
            // if the request is not send to cx after detecting the file service will send
            // a break replication request to CX.

            try 
            {
                string PendingActionsFilePath = CDPUtil::getPendingActionsFilePath(cxFormattedVolumeName, ".rollback");

                // PR#10815: Long Path support
                ACE_HANDLE h =  ACE_OS::open(getLongPathName(PendingActionsFilePath.c_str()).c_str(), 
                    O_RDWR | O_CREAT, FILE_SHARE_READ);

                if(ACE_INVALID_HANDLE == h) 
                {
                    stringstream l_stdfatal;
                    l_stdfatal << "Error detected  " << "in Function:" 
                        << FUNCTION_NAME 
                        << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                        << "Error during file (" <<  PendingActionsFilePath << ")creation.\n"
                        <<  Error::Msg() << "\n"
                        << "Failed to add " << cxFormattedVolumeName
                        << " in pending rollback action list\n"
                        << "The replication pair has to be deleted manually"
                        << " from Central Management Server UI.\n";

                    DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                } else {
                    int len = cleanupaction.size();
                    if(ACE_OS::write (h,cleanupaction.c_str(),len) != len){
                        DebugPrintf(SV_LOG_FATAL, "File: %s cannot be written\n",PendingActionsFilePath.c_str());
                    }
                    ACE_OS::close(h);
                }
            }

            catch(...)
            {
                stringstream l_stdfatal;
                l_stdfatal << "Failed to add " << cxFormattedVolumeName 
                    << " in pending rollback action list\n"
                    << "The replication pair has to be deleted manually"
                    << " from Central Management Server UI.\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            }
        }
        else
        {
            try
            {
                Configurator* TheConfigurator = 0;
                if(!GetConfigurator(&TheConfigurator))
                {
                    DebugPrintf(SV_LOG_INFO,"\nObtaining configurator failed for %s\n", cxFormattedVolumeName.c_str());
                }
                else
                {
                    bool status = false;
                    bool rv = updateVolumeAttribute(*TheConfigurator, UPDATE_NOTIFY,cxFormattedVolumeName,VOLUME_VISIBLE_RW,"",status);
                    if(!rv || !status)
                    {
                        DebugPrintf(SV_LOG_INFO,"\nUpdating volume state for target volume %s failed\n", cxFormattedVolumeName.c_str());
                    }
                    std::stringstream msg;
                    msg << "Target device: " << cxFormattedVolumeName
                        << " is rolled back by specifying '--stopreplication=no' option. "
                        << "Pair is left undeleted but resync is required for this volume." << std::endl;
                    rv = setTargetResyncRequired(*TheConfigurator, cxFormattedVolumeName, status, msg.str(), ResyncReasonStamp());
                    if(!rv || !status)
                    {
                        DebugPrintf(SV_LOG_INFO,"\nSetting ResyncRequired flag for target volume %s failed\n", cxFormattedVolumeName.c_str());
                    }
                }

            }
            catch (exception& e)
            {            
                DebugPrintf(SV_LOG_INFO,"\nObtaining configurator failed with exception %s.\n", e.what());
                //return false;
            } 
            catch(...)
            {
                DebugPrintf(SV_LOG_WARNING,"\nObtaining configurator failed with unknown exception\n");
                //return false;
            }
        }

        //added clean retention action here for making it independent of stopreplication.
        if(m_request->cleanupRetention)
        {
            std::string error;
            if(!CDPUtil::CleanRetentionLog(m_request->dbpath,error))
                DebugPrintf(SV_LOG_INFO,"%s\n",error.c_str());
        }
    }

    if(bresyncRequired && !breakReplication)
    {
        std::string cxFormattedVolumeName = m_request -> dest;
        FormatVolumeNameForCxReporting(cxFormattedVolumeName);
        const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
        std::string resync_reason = "Rollback failed after applying few changes.";
        resync_reason += " Replication pair is left undeleted but resync is required for this device.";
        resync_reason += " Marking resync for the target device " + cxFormattedVolumeName + " with resyncReasonCode=" + resyncReasonCode;;
        DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, resync_reason.c_str());
        ResyncReasonStamp resyncReasonStamp;
        STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

        CDPUtil::store_and_sendcs_resync_required(cxFormattedVolumeName, resync_reason, resyncReasonStamp);
    }
    return true;
}

bool SnapshotTask::recover()
{
    do
    {
        try
        {
            // 
            // we move to start state
            //
            m_state = SNAPSHOT_REQUEST::RecoveryStarted;
            SendStateChange();

            //
            // checks on snapshot source
            // 1) should exist
            // 2) should be hidden volume
            // 3) should be replicated volume in case of recovery/rollback (and vsnap)
            // 4) should not be in resync for physical snapshot (and pit vsnap)
            std::string sparsefile;
            bool newvirtualvolume = false; 
            bool is_volpack = IsVolPackDevice(m_request ->src.c_str(),sparsefile,newvirtualvolume);
            //skipping the existance of file existance in case of multisparse file
            //as it is taken care inside the function IsVolPackDevice
            //skipping the isvolumelocked check as we donot have to do anything for multisparse
            if(!newvirtualvolume)
            {
                if(!IsFileORVolumeExisting(m_request ->src.c_str()))
                {
                    m_err += "source volume does not exist.\n";
                    m_state = SNAPSHOT_REQUEST::Failed;
                    SendStateChange();
                    break;
                }
                // The caller should ensure that the source volume is locked (hidden) 
                // and remains so until the snapshot completes
                if(!m_skipCheck && !IsVolumeLocked(m_request ->src.c_str()))
                {
                    m_err += "recovery cannot be performed on ";
                    m_err += m_request ->src;
                    m_err += ". It is in unlocked state.\n";
                    m_state = SNAPSHOT_REQUEST::Failed;
                    SendStateChange();
                    break;
                }
            }

            if(!m_skipCheck && !isTargetVolume(m_request ->src))
            {
                m_err += "recovery cannot be perform on " ;
                m_err += m_request ->src;
                m_err += "It is not a target volume.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }        

            //checks on snapshot destination mountpoint
            // the mount point should not be "/", "/usr", "/home", "/tmp",
            //"/var", "/etc", "/var/crash", "/boot", "/sys", any directory under "/proc", 
            //any directory under "/dev" are not allowed,
            //any parent directory which can cause Vx Cache dir, Vx install dir
            std::string errmsg;
            if(!IsValidMountPoint(m_request ->destMountPoint,errmsg))
            {
                m_err += "snapshot cannot be taken on ";
                m_err += m_request->dest;
                m_err += ". ";
                m_err += errmsg;
                m_err += "\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            //
            // checks on snapshot destination
            // 1) should exist
            // 2) should not be protected volume
            // 3) should not be replicated volume
            // 4) should not contain retention files
            // 5) should not contain vsnap map files
            // 6) should not contain volpack data files
            // 7) should not be readonly
            // 8) should not contain mounted volumes
            // 9) should not contain partitions

            if(!IsFileORVolumeExisting(m_request ->dest.c_str()))
            {
                m_err += "destination volume does not exist.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }
            //Bug #5685
            //Checking if the recovery dest is valid device
            //This is needed in case mountpoint is provided from cdpcli

            if(!IsValidDevfile(m_request->dest))
            {
                m_err += "destination volume isn't a valid device.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && isProtectedVolume(m_request ->dest ))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It is a protected volume.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && isTargetVolume(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It is a target volume.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && containsRetentionFiles(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains retention data.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && containsVsnapFiles(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains vsnap data.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && containsVolpackFiles(m_request ->dest))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains virtual volume data.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            bool readonly = false;
            if(isReadOnly(m_request -> dest.c_str(), readonly).failed() || readonly)
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It is a read-only volume.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            std::string mountedvolname;
            if(!m_skipCheck && containsMountedVolumes(m_request -> dest, mountedvolname))
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += ". It contains mounted volumes";
                m_err += "(";
                m_err += mountedvolname ;
                m_err += ")\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            std::string errormsg;
            if(!m_skipCheck && !IsValidDestination(m_request -> dest, errormsg))
            {
                m_err += errormsg;
                DebugPrintf(SV_LOG_ERROR, "Invalid destination specified : %s\n", errormsg.c_str());
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            // while purge operation is in progress, we do not want
            // any recovery jobs to access the retention files
            CDPLock purge_lock(m_request -> src, true, ".purgelock");
            purge_lock.acquire_read();

            CDPLock recoverCdpLock(m_request -> src);
            if(!recoverCdpLock.acquire_read())
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += " due to aquaring read lock on the volume ";
                m_err += m_request -> src;
                m_err += " failed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            CDPLock destCdplock(m_request ->dest);
            if(!destCdplock.acquire())
            {
                m_err += "snapshot cannot be taken on " ;
                m_err += m_request ->dest;
                m_err += " due to aquaring write lock on the volume ";
                m_err += m_request ->dest;
                m_err += " failed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            // 
            // common checks
            // 1) source and target have to be different
            // 2) target capacity should be greater than source

            if(sameVolumeCheck(m_request ->src, m_request -> dest))
            {
                m_err += "snapshot source and destination cannot be same.\n" ;

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!m_skipCheck && !computeSrcCapacity(m_request -> src, m_request -> srcVolCapacity))
            {
                m_err += "source volume: ";
                m_err += m_request ->src ;
                m_err += " capacity could not be computed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if(!computDestCapacity(m_request -> dest, m_destCapacity))
            {
                m_err += "Destination volume: ";
                m_err += m_request ->dest ;
                m_err += " capacity could not be computed.\n";

                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            if( m_destCapacity < m_request -> srcVolCapacity)
            {
                m_err += "Target Volume capacity is less than Source Volume\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }



            //
            // verify rollback point
            //

            SV_TIME svtime;
            SV_ULONGLONG recoverypt;
            sscanf(m_request ->recoverypt.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                &svtime.wYear,
                &svtime.wMonth,
                &svtime.wDay,
                &svtime.wHour,
                &svtime.wMinute,
                &svtime.wSecond,
                &svtime.wMilliseconds,
                &svtime.wMicroseconds,
                &svtime.wHundrecNanoseconds);
            if(!CDPUtil::ToFileTime(svtime, recoverypt))
            {
                m_err += "specified rollback point is invalid\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            bool isconsistent = false;
            bool isavailable = false;

            std::string displaytime;
            CDPUtil::CXInputTimeToConsoleTime(m_request ->recoverypt, displaytime);

            if( !CDPUtil::isCCP(m_request->dbpath,recoverypt,isconsistent, isavailable)|| !isavailable )
            {
                std::stringstream errstring;
                errstring << " The specified time " << displaytime << " is not a valid recovery point.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                m_err += errstring.str();
                SendStateChange();
                break;
            }

            if(!isconsistent)
            {
                std::stringstream infostring;
                infostring << " The specified time " << displaytime << " is not a crash consistent point.\n";
                m_state = SNAPSHOT_REQUEST::RecoveryStarted;
                m_err += infostring.str();
                SendStateChange();
            }


            //run pre script
            if(!runprescript())
            {
                m_errno = CDPPreScriptFailed;
                failImmediate();
                break;
            }

            // move to copy in progress state
            m_state = SNAPSHOT_REQUEST::RecoverySnapshotPhaseInProgress;
            SendStateChange();
            // get read access to source volume
            cdp_volume_t srcvol(m_request ->src,newvirtualvolume);

            // Enable Retries on failures (required for fabric multipath device failures)
            srcvol.EnableRetry(CDPUtil::QuitRequested,GetRetries(), GetRetryDelay());

            BasicIo::BioOpenMode  srcvol_mode= (BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary| BasicIo::BioSequentialAccess);
            if(m_directio)
            {
#ifdef SV_WINDOWS
                srcvol_mode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
                srcvol_mode |= BasicIo::BioDirect;
#endif
            }

            srcvol.Open(srcvol_mode);
            if (!srcvol.Good()) 
            {
                stringstream msg;
                msg << "failed to open source volume :" 
                    << srcvol.GetName()<< " error: " << srcvol.LastError() << '\n';
                m_err += msg.str();
                m_errno = CDPSrcOpenFailed;
                fail();
                break;
            }

            // get exclusive access (hide and exclusive) to target volume
            cdp_volume_t tgtvol(m_request ->dest);

            // Enable Retries on failures (required for fabric multipath device failures)
            tgtvol.EnableRetry(CDPUtil::QuitRequested,GetRetries(), GetRetryDelay());

            if(!tgtvol.OpenExclusive() || !tgtvol.Good())
            {
                stringstream msg;
                msg << "failed to open target volume :" 
                    << tgtvol.GetName()<< " error: " << tgtvol.LastError() << '\n';
                m_err += msg.str();
                m_errno = CDPTgtOpenFailed;
                fail();
                break;
            }

            std::string filesystem = getFSType(m_request ->src);
            boost::tribool res = false;

            if(m_use_fsawarecopy)
            {
                res = copydata_forfsaware_volumes(srcvol,tgtvol,  m_request ->srcVolCapacity, filesystem);
            }

            //If fsawarecopy is not enabled or fsaware copy is not supported or failed, invoke the normal copydata.
            if((!m_use_fsawarecopy || FsAwareCopyNotSupported(res) || !res))
            {
                if(!copydata(srcvol,tgtvol,  m_request ->srcVolCapacity))
                {
                    srcvol.Close();
                    tgtvol.Close();
                    fail();
                    break;
                }
            }

            srcvol.Close();

            // rollback phase

            m_state = SNAPSHOT_REQUEST::RecoveryRollbackPhaseStarted;
            SendStateChange();

            m_state = SNAPSHOT_REQUEST::RecoveryRollbackPhaseInProgress;
            SendStateChange();

            //
            // get database handle
            //

            CDPDatabase database(m_request->dbpath);
            CDPSummary summary;
            if(!database.getcdpsummary(summary))
            {
                m_err += "Error: read failure from retention store \n";
                m_errno = CDPReadFromRetentionFailed;
                fail();
                break;
            }


            //
            // verify rollback point is within retention window range
            //

            string datefrom, dateto, recoveryto;
            if(!CDPUtil::ToDisplayTimeOnConsole(summary.start_ts, datefrom))
            {
                m_err += "Error: read failure from retention store \n";
                m_errno = CDPReadFromRetentionFailed;
                fail();
                break;
            }

            if(!CDPUtil::ToDisplayTimeOnConsole(summary.end_ts, dateto))
            {
                m_err += "Error: read failure from retention store \n";
                m_errno = CDPReadFromRetentionFailed;
                fail();
                break;
            }

            if(!CDPUtil::ToDisplayTimeOnConsole(recoverypt, recoveryto))
            {
                m_err += "Error: specified time is invalid\n";
                m_errno = CDPReadFromRetentionFailed;

            }

            if(recoverypt < summary.start_ts || recoverypt > summary.end_ts)
            {
                m_err += "The specified time: ";
                m_err += recoveryto;
                m_err += " is not within available recovery time range (";
                m_err += datefrom;
                m_err += " to ";
                m_err += dateto;
                m_err += ").\n";
                fail();
                break;
            } 

            CDPDRTDsMatchingCndn cndn;

            //
            // verify specified tag is available
            //
            cndn.toEvent(m_request ->recoverytag); 
            cndn.toTime(recoverypt);
            cndn.toSequenceId(m_request->sequenceId);


            // 
            // transfer contents from retention log to the target volume ...
            //

            if(!rollback(database, cndn, tgtvol, summary.start_ts, summary.end_ts))
            {
                tgtvol.Close();
                fail();
                break;
            }

#ifdef SV_SUN

            // On sun, we are opening in buffered mode. so we need to flush
            if(ACE_INVALID_HANDLE != tgtvol.GetHandle())
            {
                if(ACE_OS::fsync(tgtvol.GetHandle()) == -1)
                {
                    stringstream msg;
                    msg << "failed in flush writes to target volume :" 
                        << tgtvol.GetName()<< " error: " << ACE_OS::last_error() << '\n';
                    m_err += msg.str();
                    m_errno = CDPWriteToTgtFailed;
                    tgtvol.Close();
                    fail();
                    break;
                }
            }
#endif

            tgtvol.Close();

            // 
            // unhide the destination volume
            //
            string fsType = getFSType(m_request ->src);
            if(!makeVolumeAvailable(m_request ->dest,m_request->destMountPoint,fsType))
            {
                fail();
                break;
            }


            // run postscript
            if(!runpostscript())
            {
                m_state = SNAPSHOT_REQUEST::Failed;
                SendStateChange();
                break;
            }

            m_state = SNAPSHOT_REQUEST::Complete;
            SendStateChange();
        }catch ( ContextualException& ce )
        {
            std::string errormsg = "snapshot cannot be taken. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered exception ";
            errormsg += ce.what();
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }
        catch( exception const& e )
        {
            std::string errormsg = "snapshot cannot be taken. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered exception ";
            errormsg += e.what();
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }
        catch ( ... )
        {
            std::string errormsg = "snapshot cannot be taken. ";
            errormsg += FUNCTION_NAME;
            errormsg += " encountered unknown exception.";
            m_err += errormsg;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            m_state = SNAPSHOT_REQUEST::Failed;
            SendStateChange();
            break;
        }

    } while (0);

    return true;
}

bool SnapshotTask::vsnap_mount()
{

    VsnapMgr *vsnapmgr;
#ifdef SV_WINDOWS
    WinVsnapMgr obj;
    vsnapmgr=&obj;
#else
    UnixVsnapMgr obj;
    vsnapmgr=&obj;
#endif

    //#ifdef SV_WINDOWS
#if 1 // EVENT DRIVEN SNAPSHOTS
    try {


        boost::shared_ptr<VsnapSourceVolInfo> srcvol(new VsnapSourceVolInfo());
        boost::shared_ptr<VsnapVirtualVolInfo> tgtvol(new VsnapVirtualVolInfo());

        SV_TIME svtime;
        SV_ULONGLONG recoverypt = 0;
        if(!m_request ->recoverypt.empty()) 
        {

            sscanf(m_request ->recoverypt.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                &svtime.wYear,
                &svtime.wMonth,
                &svtime.wDay,
                &svtime.wHour,
                &svtime.wMinute,
                &svtime.wSecond,
                &svtime.wMilliseconds,
                &svtime.wMicroseconds,
                &svtime.wHundrecNanoseconds);

            if(!CDPUtil::ToFileTime(svtime, recoverypt))    {
                m_state = SNAPSHOT_REQUEST::Failed;
                m_err += std::string("specified rollback point is invalid");
                SendStateChange();
                return false;
            }
            tgtvol -> TimeToRecover = recoverypt;
        }

        tgtvol -> EventName = m_request ->recoverytag;
        tgtvol -> SequenceId = m_request ->sequenceId;
        // set the object for monitoring quit request
        vsnapmgr->SetSnapshotObj(this);

        // fill in the virtual volume


        // VSNAPFC - Persistence
        // In case of Linux,
        // SNAPSHOT_REQUEST->destMountPoint - contains either mountpoint like /mnt/vsnap1 or "" if not provided
        // SNAPSHOT_REQUEST->dest - contains autogenerated device name for e.g /dev/vs/cx0

#ifndef SV_WINDOWS        

        if(m_request -> dest.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s\t%s\t%d\tDevice name is empty.\n", __FILE__, __FUNCTION__, __LINE__);
            std::string errstring;
            errstring += " Device name is empty. \n";
            m_state = SNAPSHOT_REQUEST::Failed;
            m_err += errstring;
            SendStateChange();
            return false;
        }        

        if(m_request -> dest.substr(0, 8) != "/dev/vs/")
        {
            DebugPrintf(SV_LOG_DEBUG, "%s\t%s\t%d\tDevice Name isn't in proper format\n Device Name = %s\n", __FILE__, __FUNCTION__, __LINE__ ,m_request -> dest.c_str());
            std::stringstream errstring;
            errstring << " Device Name " <<m_request ->dest << " isn't in proper format. \n";
            m_state = SNAPSHOT_REQUEST::Failed;
            m_err += errstring.str();
            SendStateChange();
            return false;
        }

        if(m_request -> dest == m_request -> destMountPoint)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s\t%s\t%d\t***ERROR***Mount Point = %s and Device Name = %s are same\n", __FILE__, __FUNCTION__, __LINE__ ,m_request -> destMountPoint.c_str(), m_request -> dest.c_str());
            std::stringstream errstring;
            errstring << " Mount Point = " << m_request -> destMountPoint << " and Device Name = " <<m_request ->dest << " are same. \n";
            m_state = SNAPSHOT_REQUEST::Failed;
            m_err += errstring.str();
            SendStateChange();
            return false;
        }

        tgtvol -> VolumeName = m_request -> destMountPoint;
        tgtvol -> DeviceName = m_request -> dest;

#else 

        tgtvol -> VolumeName = m_request -> dest;

#endif

        if(*(tgtvol->VolumeName.c_str()) == '\\')
        {
            tgtvol->VolumeName.replace(0,4,"");
            if((tgtvol->VolumeName.size() > 3) && 
                (*(tgtvol->VolumeName.c_str() + tgtvol->VolumeName.size() - 1) == ':'))
                tgtvol->VolumeName.replace(tgtvol->VolumeName.size() - 1, 1, "");
        }

        //UnformatVolumeNameToDriveLetter(tgtvol ->VolumeName);
        std::string dataLogPath = m_request -> vsnapdatadirectory;
#ifdef SV_WINDOWS
        if(dataLogPath.length() == 1) //If only letter(drive) is passed 
        {
            dataLogPath += std::string(":"); //Append colon to drive letter
        }
#endif
        tgtvol -> PrivateDataDirectory = dataLogPath;
        tgtvol -> AccessMode = static_cast<VSNAP_VOLUME_ACCESS_TYPE>(m_request -> vsnapMountOption);
        tgtvol -> PreScript = m_request -> prescript;
        tgtvol -> PostScript = m_request -> postscript;
        if(m_request -> operation == SNAPSHOT_REQUEST::PIT_VSNAP)
            tgtvol -> ReuseDriveLetter = true;

        // fill in the source volume
        srcvol-> vsnap_id = this->m_id;
        srcvol -> VolumeName = m_request -> src;
        if ( IsDrive(srcvol -> VolumeName))
        {
            UnformatVolumeNameToDriveLetter(srcvol ->VolumeName);
        }            


        std::string dbpath = m_request -> dbpath;

        if(!dbpath.empty())
        {
            // convert the path to real path (no symlinks)
            if( !resolvePathAndVerify( dbpath ) )
            {
                DebugPrintf( SV_LOG_ERROR,
                    "Retention Database Path %s not yet created\n",
                    dbpath.c_str() ) ;
                std::stringstream errstring;
                errstring << " Retention Database Path :" << dbpath << " not yet created.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                m_err += errstring.str();
                SendStateChange();
                return false;
            }
        }

        if(!m_request -> recoverypt.empty())
        {
            bool isconsistent = false;
            bool isavailable = false;

            std::string displaytime;
            CDPUtil::CXInputTimeToConsoleTime(m_request ->recoverypt, displaytime);

            if( !CDPUtil::isCCP(dbpath,tgtvol -> TimeToRecover,isconsistent, isavailable)|| !isavailable )
            {
                std::stringstream errstring;
                errstring << " The specified time " << displaytime << " is not a valid recovery point.\n";
                m_state = SNAPSHOT_REQUEST::Failed;
                m_err += errstring.str();
                SendStateChange();
                return false;
            }

            if(!isconsistent)
            {
                std::stringstream infostring;
                infostring << " The specified time " << displaytime << " is not a crash consistent point.\n";
                m_state = SNAPSHOT_REQUEST::Ready;
                m_err += infostring.str();
                SendStateChange();
            }
        }

        srcvol -> RetentionDbPath = dbpath;

        srcvol -> VirtualVolumeList.push_back(tgtvol.get());

        vsnapmgr->SetCXSettings();
        vsnapmgr->Mount(srcvol.get());
    } catch (exception & )    {
        // do nothing
    }
#endif

    m_state = SNAPSHOT_REQUEST::Complete;
    SendStateChange();
    //#endif
    return true;
}

bool SnapshotTask::vsnap_unmount()
{
    VsnapMgr *vsnapmgr;
#ifdef SV_WINDOWS
    WinVsnapMgr obj;
    vsnapmgr=&obj;
#else
    UnixVsnapMgr obj;
    vsnapmgr=&obj;
#endif

    //#ifdef SV_WINDOWS
#if 1 // EVENT DRIVEN SNAPSHOTS
    try {
        //VsnapMgr *vsnapmgr;
        //WinVsnapMgr obj;
        //vsnapmgr =&obj;
        boost::shared_ptr<VsnapVirtualVolInfo> tgtvol(new VsnapVirtualVolInfo());
        VsnapVirtualInfoList virtualInfos;


        // fill in the virtual volume


        // VSNAPFC - Persistence
        // In case of Linux,
        // SNAPSHOT_REQUEST->destMountPoint - contains either mountpoint like /mnt/vsnap1 or "" if not provided
        // SNAPSHOT_REQUEST->dest - contains autogenerated device name for e.g /dev/vs/cx0

#ifndef SV_WINDOWS        

        tgtvol -> VolumeName = m_request -> destMountPoint;
        tgtvol -> DeviceName = m_request -> dest;

#else 

        tgtvol -> VolumeName = m_request -> dest;

#endif


        if(*(tgtvol->VolumeName.c_str()) == '\\')
        {
            tgtvol->VolumeName.replace(0,4,"");
            if((tgtvol->VolumeName.size() > 3) && 
                (*(tgtvol->VolumeName.c_str() + tgtvol->VolumeName.size() - 1) == ':'))
                tgtvol->VolumeName.replace(tgtvol->VolumeName.size() - 1, 1, "");
        }
        else if((tgtvol->VolumeName.size() > 3) && 
            (*(tgtvol->VolumeName.c_str() + tgtvol->VolumeName.size() - 1) == ':'))
            tgtvol->VolumeName.replace(tgtvol->VolumeName.size() - 1, 1, "");

        //UnformatVolumeNameToDriveLetter(tgtvol ->VolumeName);
        virtualInfos.push_back(tgtvol.get());

        vsnapmgr->SetCXSettings();
        vsnapmgr->Unmount(virtualInfos, true);
    } catch (exception & )    {
        //do nothing
    }
#endif
    m_state = SNAPSHOT_REQUEST::Complete;
    SendStateChange();
    //#endif
    return true;
}

bool SnapshotTask::SendStateChange()
{
    try {
        CDPMessage * msg = new CDPMessage();

        msg -> id = this ->m_id;
        msg -> operation = m_request -> operation;
        msg -> src = m_request -> src;

        if(m_request -> operation == SNAPSHOT_REQUEST::RECOVERY_VSNAP)
        {
            if(m_request -> destMountPoint.empty())
            {
                msg -> dest = m_request -> dest;
            }
            else
            {
                msg -> dest = m_request -> destMountPoint;
            }
        }
        else
        {
            msg -> dest = m_request -> dest;
        }
        msg -> type = CDPMessage::MSG_STATECHANGE;
        msg -> executionState = this -> m_state;
        msg -> progress = this -> m_progress;
        msg -> err = this -> m_err;

        ACE_Message_Block *entry = new ACE_Message_Block ((char *) msg, sizeof(CDPMessage));
        m_stateMgr ->enqueue(entry);
    }catch( exception & e)    {
        DebugPrintf(SV_LOG_ERROR, "%s", e.what());
        return false;
    }

    return true;
}

bool SnapshotTask::SendProgress()
{
    try {
        CDPMessage * msg = new CDPMessage();

        msg -> id = this ->m_id;
        msg -> operation = m_request -> operation;
        msg -> src = m_request -> src;
        if(m_request -> operation == SNAPSHOT_REQUEST::RECOVERY_VSNAP)
        {
            if(m_request -> destMountPoint.empty())
            {
                msg -> dest = m_request -> dest;
            }
            else
            {
                msg -> dest = m_request -> destMountPoint;
            }
        }
        else 
        {
            msg -> dest = m_request -> dest;
        }
        msg -> type = CDPMessage::MSG_PROGRESSUPDATE;
        msg -> executionState = this -> m_state;
        msg -> progress = this -> m_progress;
        msg -> time_remaining_in_secs = this ->m_time_remaining_in_secs;
        msg -> err = this -> m_err;

        ACE_Message_Block *entry = new ACE_Message_Block ((char *) msg, sizeof(CDPMessage));
        m_stateMgr ->enqueue(entry);
    }catch( exception & e)    {
        DebugPrintf(SV_LOG_ERROR, "%s", e.what());
        return false;
    }

    return true;
}

// failImmediate differs from fail in the sense that
// it does not execute postscript
void SnapshotTask::failImmediate()
{
    m_state = SNAPSHOT_REQUEST::Failed;
    SendStateChange();
}

void SnapshotTask::fail()
{
    if(!Quit())
        runpostscript();
    m_state = SNAPSHOT_REQUEST::Failed;
    SendStateChange();
}

bool SnapshotTask::runprescript()
{
    std::string prescript = m_request -> prescript;


    if (prescript.empty() || '\0' == prescript[0] || "\n" == prescript )
        return true;

    CDPUtil::trim(prescript);
    string stuff_to_trim = " \n\b\t\a\r\xc" ;
    if (prescript.find_first_not_of(stuff_to_trim) == string::npos)
        return true;

    switch(m_request -> operation)
    {
    case SNAPSHOT_REQUEST::PLAIN_SNAPSHOT:
        m_state = SNAPSHOT_REQUEST::SnapshotPrescriptRun;
        break;

    case SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT:
        m_state = SNAPSHOT_REQUEST::RecoveryPrescriptRun;
        break;

    case SNAPSHOT_REQUEST::ROLLBACK:
        m_state = SNAPSHOT_REQUEST::RollbackPrescriptRun;
        break;

    default:
        break;
    }
    SendStateChange();

    std::ostringstream args;
    args << prescript 
        << " --S " << '"' << m_request ->src << '"'
        << " --T " << '"' << m_request ->dest << '"' ;


    InmCommand script(args.str());
    script.SetShouldWait(false);
    InmCommand::statusType status = script.Run();
    if(status != InmCommand::running)
    {
        std::ostringstream msg;
        msg << "internal error occured during execution of the prescript: " << prescript << std::endl;
        if(!script.StdErr().empty())
        {
            msg << script.StdErr();
        }
        m_err += msg.str();
        return false;
    }

    std::string output;
    static const int TaskWaitTimeInSeconds = 5;
    std::ostringstream msg;
    while(true)
    {
        if (Quit(TaskWaitTimeInSeconds)) {
            script.Terminate();
            msg << "prescript terminated because of service shutdown request";
            m_err += msg.str();
            return false;
        }

        status = script.TryWait(); // just check for exit don't wait
        if (status == InmCommand::internal_error) 
        {
            msg << "internal error occured during execution of the prescript: " << prescript << std::endl;
            if(!script.StdErr().empty())
            {
                msg << script.StdErr();
            }
            m_err += msg.str();
            return false;
        } 
        else if (status == InmCommand::completed) 
        {

            if(script.ExitCode())
            {
                msg << "prescript failed : Exit Code = " << script.ExitCode() << std::endl;
                if(!script.StdOut().empty())
                {
                    msg << "Output = " << script.StdOut() << std::endl;
                }
                if(!script.StdErr().empty())
                {
                    msg << "Error = " << script.StdErr()<< std::endl;
                }
                m_err += msg.str();
                return false;
            }
            return true;
        }
    }
}

bool SnapshotTask::runpostscript()
{
    std::string postscript = m_request -> postscript;


    if (postscript.empty() || '\0' == postscript[0] || "\n" == postscript )
        return true;

    CDPUtil::trim(postscript);
    string stuff_to_trim = " \n\b\t\a\r\xc" ;
    if (postscript.find_first_not_of(stuff_to_trim) == string::npos)
        return true;

    switch(m_request -> operation)
    {
    case SNAPSHOT_REQUEST::PLAIN_SNAPSHOT:
        m_state = SNAPSHOT_REQUEST::SnapshotPostscriptRun;
        break;

    case SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT:
        m_state = SNAPSHOT_REQUEST::RecoveryPostscriptRun;
        break;

    case SNAPSHOT_REQUEST::ROLLBACK:
        m_state = SNAPSHOT_REQUEST::RollbackPostScriptRun;
        break;

    default:
        break;
    }
    SendStateChange();

    std::ostringstream args;
    args << postscript
        << " --S " << '"' << m_request ->src << '"'
        << " --T " << '"' << m_request ->dest << '"'
        << " --E " << m_errno;

    InmCommand script(args.str());
    script.SetShouldWait(false);
    InmCommand::statusType status = script.Run();
    if(status != InmCommand::running)
    {
        std::ostringstream msg;
        msg << "internal error occured during execution of the postscript: " << postscript << std::endl;
        if(!script.StdErr().empty())
        {
            msg << script.StdErr();
        }
        m_err += msg.str();
        return false;
    }

    std::string output;
    static const int TaskWaitTimeInSeconds = 5;
    std::ostringstream msg;
    while(true)
    {
        if (Quit(TaskWaitTimeInSeconds)) {
            script.Terminate();
            msg << "postscript terminated because of service shutdown request";
            m_err += msg.str();
            return false;
        }

        status = script.TryWait(); // just check for exit don't wait
        if (status == InmCommand::internal_error) 
        {
            msg << "internal error occured during execution of the postscript: " << postscript << std::endl;
            if(!script.StdErr().empty())
            {
                msg << script.StdErr();
            }
            m_err += msg.str();
            return false;
        } 
        else if (status == InmCommand::completed) 
        {

            if(script.ExitCode())
            {
                msg << "postscript failed : Exit Code = " << script.ExitCode() << std::endl;
                if(!script.StdOut().empty())
                {
                    msg << "Output = " << script.StdOut() << std::endl;
                }
                if(!script.StdErr().empty())
                {
                    msg << "Error = " << script.StdErr()<< std::endl;
                }
                m_err += msg.str();
                return false;
            }
            return true;
        }
    }
}    



bool SnapshotTask::WaitForProcess(ACE_Process_Manager * pm, pid_t pid)
{
    std::ostringstream msg;

    ACE_Time_Value timeout;
    ACE_exitcode status = 0;
    static const int TaskWaitTimeInSeconds = 5;

    while (true) 
    {

        if (Quit(TaskWaitTimeInSeconds)) {
            TerminateProcess(pm, pid);
            msg << "script terminated beacuse of service shutdown request";
            m_err += msg.str();
            return false;
        }

        pid_t rc = pm->wait(pid, timeout, &status); // just check for exit don't wait
        if (ACE_INVALID_PID == rc) 
        {
            msg << "script failed in ACE_Process_Manager::wait with error no: " 
                << ACE_OS::last_error() << '\n';
            m_err += msg.str();
            return false;
        } 
        else if (rc == pid) 
        {
            if (0 != status) 
            {
                msg << "script failed with exit status " << status << '\n';
                m_err += msg.str();
                return false;
            }
            return true;
        }
    }
    return true;
}



/*
* FUNCTION NAME :  GetRetries     
*
* DESCRIPTION :    Get the number of retries from localconfigurator
*                  
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : NONE
*
*/
unsigned int SnapshotTask::GetRetries()
{
    unsigned int retries = 0;
    try 
    {
        retries = m_localConfigurator.getVolumeRetries();
    }
    catch(...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to get retry count.\n");
    }

    return retries;
}

/*
* FUNCTION NAME :  GetRetryDelay     
*
* DESCRIPTION :    Get retry delay in seconds from localconfigurator
*                  
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : NONE
*
*/
unsigned int SnapshotTask::GetRetryDelay()
{
    unsigned int retryDelay = 0;
    try 
    {
        retryDelay = m_localConfigurator.getVolumeRetryDelay();
    }
    catch(...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to get retry delay.\n");
    }

    return retryDelay;
}


bool SnapshotTask::IsDiskOrPartitionValidDestination(const std::set<std::string> & disknames, std::string &errormsg) 
{
    bool rv = true;
    std::set<std::string>::const_iterator volumeIterator(disknames.begin());
    std::set<std::string>::const_iterator volumeIteratorEnd(disknames.end());
    for( ; volumeIterator != volumeIteratorEnd; volumeIterator++)
    {
        std::string mountpoint;
        std::string mountmode;
        if (isProtectedVolume(*volumeIterator))
        {
            errormsg = "It is a partition of a replicated source disk ";
            errormsg += (*volumeIterator);
            errormsg += "\n";
            rv = false;
            break;
        }
        else if (isTargetVolume(*volumeIterator))
        {
            errormsg += "It is a partition of a replicated target disk ";
            errormsg += (*volumeIterator);
            errormsg += "\n";
            rv = false;
            break;
        }
        else if (IsVolumeMounted(*volumeIterator, mountpoint, mountmode))
        {
            errormsg = "It is a partition of disk ";
            errormsg += (*volumeIterator);
            errormsg += " mounted on mountpoint ";
            errormsg += mountpoint;
            errormsg += "\n";
            rv = false;
            break;
        }
    }
    return rv;
}

bool SnapshotTask::IsValidDeviceToProceed(const std::string & volumename,std::string &errormsg)
{
    bool rv = true;
    do
    {
        if (isProtectedVolume(volumename))
        {
            errormsg += "It is a partition of a replicated source disk ";
            errormsg += volumename;
            errormsg += "\n";
            rv = false;
            break;
        }
        if (isTargetVolume(volumename))
        {
            errormsg += "It is a partition of a replicated target disk ";
            errormsg += volumename;
            errormsg += "\n";
            rv = false;
            break;
        }
        if(containsRetentionFiles(volumename))
        {
            errormsg = "The device ";
            errormsg += volumename;
            errormsg += " contains retention data.\n";
            rv = false;
            break;
        }

        if(containsVsnapFiles(volumename))
        {
            errormsg = "The device ";
            errormsg += volumename;
            errormsg += " contains vsnap data.\n";
            rv = false;
            break;
        }

        if(containsVolpackFiles(volumename))
        {
            errormsg = "The device ";
            errormsg += volumename;
            errormsg += " contains virtual volume data.\n";
            rv = false;
            break;
        }
        std::string mountedvolname;
        if(containsMountedVolumes(volumename, mountedvolname))
        {
            errormsg = "The device ";
            errormsg += volumename;
            errormsg += " contains mounted volumes";
            errormsg += "(";
            errormsg += mountedvolname ;
            errormsg += ")\n";
            rv = false;
            break;
        }
        if(IsInstallPathVolume(volumename))
        {
            errormsg = "The device ";
            errormsg += volumename;
            errormsg += " is a Vx install volume.\n";
            rv = false;
            break;
        }
    } while(false);
    return rv;
}





std::string SnapshotTask::GetVsnapVolumeName(const std::string & vol,const std::string & dest)
{
    std::string vsnapname = vol;
    vsnapname += "_";
    vsnapname += dest;
    vsnapname += "_vsnap";
    replace_nonsupported_chars(vsnapname);
    return vsnapname;
}

bool SnapshotTask::SetVolumesCache()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    const int WAIT_SECS_FOR_CACHE = 10;
    bool rv = false;

    do
    {
        DataCacher dataCacher;
        if (dataCacher.Init()) {
            DataCacher::CACHE_STATE cs = dataCacher.GetVolumesFromCache(m_VolumesCache);
            if (DataCacher::E_CACHE_VALID == cs) {
                DebugPrintf(SV_LOG_DEBUG, "volume summaries are present in local cache.\n");
                rv = true;
                break;
            }
            else if (DataCacher::E_CACHE_DOES_NOT_EXIST == cs) {
                DebugPrintf(SV_LOG_DEBUG, "volume summaries are not present in local cache. Waiting for it to be created.\n");
            }
            else {
                DebugPrintf(SV_LOG_ERROR, "%s failed getting volumeinfocollector cache with error: %s @LINE %d in FILE %s.\n",
                    FUNCTION_NAME, dataCacher.GetErrMsg().c_str(), LINE_NO, FILE_NAME);
            }
        }
        else {
            DebugPrintf(SV_LOG_ERROR, "%s failed to initialize data cacher to get volumes cache for volume %s @LINE %d in FILE %s.\n",
                FUNCTION_NAME, LINE_NO, FILE_NAME);
        }
    } while (!CDPUtil::QuitRequested(WAIT_SECS_FOR_CACHE));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}


void SnapshotTask::GetDeviceProperties(const std::string & name, 
    std::string & displayName, 
    cdp_volume_t::CDP_VOLUME_TYPE & devType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    displayName = name;
    devType = cdp_volume_t::CDP_REAL_VOLUME_TYPE;
    


    //Get volume report from vic cache
    VolumeReporter::VolumeReport_t vr;
    VolumeReporterImp rptr;
    vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
    rptr.GetVolumeReport(name, m_VolumesCache.m_Vs, vr);
    rptr.PrintVolumeReport(vr);

    //Verify the disk is reported
    if (vr.m_bIsReported) {
        devType = cdp_volume_t::GetCdpVolumeType(vr.m_VolumeType);

        if (!vr.m_DeviceName.empty())
            displayName = vr.m_DeviceName;
    }

    DebugPrintf(SV_LOG_DEBUG, "Device %s display name = %s type = %s\n",
        name.c_str(), displayName.c_str(), cdp_volume_t::GetStrVolumeType(devType).c_str());


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void SnapshotTask::Initialize_RollbackStats()
{
    m_rollbackstats.metadatafile_read_bytes = 0;
    m_rollbackstats.metadatafile_read_io = 0;
    m_rollbackstats.retentionfile_read_io = 0;
    m_rollbackstats.tgtvol_write_io = 0;
    m_rollbackstats.retentionfile_read_bytes = 0;
    m_rollbackstats.tgtvol_write_bytes = 0;
    m_rollbackstats.firstchange_timeStamp = 0;
    m_rollbackstats.lastchange_timeStamp = 0;
    m_rollbackstats.rollback_operation_starttime = "";
    m_rollbackstats.rollback_operation_endtime = "";
    m_rollbackstats.Recovery_Period = 0;
}

RollbackStats_t SnapshotTask::get_io_stats()
{
    return m_rollbackstats;
}