// Copyright (c) 2005 InMage
//
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : volumeinfo.cpp
//
// Description:
//

#include <sstream>
#include "volumeinfo.h"
#include "globs.h"
#include "theconfigurator.h"
#include "cdputil.h"
#include "configurevxagent.h"
#include "configwrapper.h"
#include "localconfigurator.h"
#include "defaultdirs.h"
#include "genericprofiler.h"
using namespace std;



static std::string const SyncDir("/resync/");

// --------------------------------------------------------------------------
// constructor
// --------------------------------------------------------------------------
VolumeInfo::VolumeInfo(std::string const & name,
                       std::string const & mountPoint,
                       TRANSPORT_PROTOCOL protocol,
                       bool secure,
                       bool diffSync,
                       bool visible,
                       std::string const & diffLocation,
                       std::string const & cacheLocation,
                       bool resyncRequiredFlag, 
                       VOLUME_SETTINGS::RESYNCREQ_CAUSE resyncRequiredCause,
                       SV_ULONGLONG resyncRequiredTimestamp,
                       bool quasi,
                       SV_ULONGLONG quasiStartTime,
                       SV_ULONG     quasiStartTimeSeq,
                       SV_ULONGLONG quasiEndTime,
                       SV_ULONG     quasiEndTimeSeq,
                       bool isAzureTarget,
                       const VolumeSummaries_t *pVolumeSummariesCache)
    : m_Protocol(protocol),
      m_Secure(secure),
      m_DiffSyncMode(diffSync),
      m_Visible(visible),
      m_Name(name),
      m_MountPoint(mountPoint),
      m_TotalDiffDataSize(0),
      m_Quasi(quasi),
      m_QuasiStartTime(quasiStartTime),
      m_QuasiStartTimeSeq(quasiStartTimeSeq),
      m_QuasiEndTime(quasiEndTime),
      m_QuasiEndTimeSeq(quasiEndTimeSeq),
      m_isAzureTarget(isAzureTarget),
      m_pVolumeSummariesCache(pVolumeSummariesCache)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_DiffLocation = diffLocation + '/' + GetVolumeDirName(name);

    if(protocol == TRANSPORT_PROTOCOL_FILE)
    {
        //For fixing bug# 13260, caused due to missing drive letter on windows
        m_CacheLocation = GetPSInstallPathFromRegistry() + ACE_DIRECTORY_SEPARATOR_CHAR_A + TheConfigurator->getHostId() + ACE_DIRECTORY_SEPARATOR_CHAR_A + GetVolumeDirName(name);

    } else
    {
        m_CacheLocation = cacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A + GetVolumeDirName(name);
    }

    m_FormattedName = m_Name;
    FormatVolumeName(m_FormattedName);
    m_Rollback = false;
    m_ResyncLocation = m_DiffLocation + SyncDir ; 
    m_resyncRequiredFlag = resyncRequiredFlag;
    m_resyncRequiredCause  = resyncRequiredCause;
    m_resyncRequiredTimestamp = resyncRequiredTimestamp;    
    m_lastTimeWhenCxUpdated =  0;
    m_MemoryUsage =0;
    m_bCheckCacheDirSpace =0;

    if (m_isAzureTarget) {

        m_azureagent_ptr.reset(new AzureAgentInterface::AzureAgent(
            TheConfigurator->GetEndpointHostName(m_Name),
            TheConfigurator->GetEndpointHostId(m_Name),
            TheConfigurator->GetEndpointDeviceName(m_Name),
            TheConfigurator->GetEndpointRawSize(m_Name),
            TheConfigurator->getVxAgent().getFastSyncHashCompareDataSize(),
            TheConfigurator->GetResyncJobId(m_Name),
            (AzureAgentInterface::AzureAgentImplTypes) TheConfigurator->getVxAgent().getAzureImplType(),
            TheConfigurator->getVxAgent().getMaxAzureAttempts(),
            TheConfigurator->getVxAgent().getAzureRetryDelayInSecs()));
    }
    //if((SV_LINUX_OS == TheConfigurator->getSourceOSVal(m_Name)) || (SV_SUN_OS == TheConfigurator->getSourceOSVal(m_Name)))
    //{
    //    m_QuasiEndTime += (static_cast<SV_ULONGLONG>(116444736) * static_cast<SV_ULONGLONG>(1000000000));
    //}
}

void VolumeInfo::SetShouldCheckForCacheDirSpace()
{
    // m_bCheckCacheDirSpace values:
    // 0 - uninitialized
    // 1 - should check 
    // 2 - need not check, using file tal and cachedir and remotedir on same partition

    do {
        if(0 == m_bCheckCacheDirSpace)
        {
            if(m_Protocol != TRANSPORT_PROTOCOL_FILE ) {
                m_bCheckCacheDirSpace = 1;
                break;
            }

            if(OnSameRootVolume(m_CacheLocation, m_DiffLocation)) {
                m_bCheckCacheDirSpace = 2;
                break;
            }

            m_bCheckCacheDirSpace = 1;
        }

    } while (0);
}

bool VolumeInfo::ShouldCheckForCacheDirSpace()
{
    return ( m_bCheckCacheDirSpace == 1);
}

void VolumeInfo::Add(DiffInfo::DiffInfoPtr diffInfo)
{ 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_DiffInfos.insert(diffInfo);
    /*
    // catch this in debug
    assert(ok);
    // report in release
    if (!ok) {
    std::ostringstream msg;
    msg << "FAILED VolumeInfo::Add insert duplicate DiffInfo: " << diffInfo->Id() << '\n';
    throw DataProtectionException(msg.str());
    }*/
}

bool VolumeInfo::OpenVolumeExclusive()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    AutoLock SetVolumelock( m_VolumeLock );
    if(!m_Volume)  {
        std::string dev_name(m_FormattedName);
        std::string mntPt(m_MountPoint);
        std::string sparsefile;
        bool isnewvirtualvolume = false;
        if(!IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(),sparsefile,isnewvirtualvolume)))
        {
            dev_name = sparsefile;
            mntPt = sparsefile;
        }
        m_Volume.reset(new cdp_volume_t(dev_name,isnewvirtualvolume));
    }

    // cdp_volume_t::OpenExclusive will first check 
    // if volume was previously opened, if it is
    // it will not open again
    if (!m_Volume->OpenExclusive()) {
        return false;
    }

    // Enable Retries on failures (required for fabric multipath device failures)
    ConfigureVxAgent & vxAgent = TheConfigurator->getVxAgent();
    m_Volume ->EnableRetry(CDPUtil::QuitRequested, vxAgent.getVolumeRetries(), vxAgent.getVolumeRetryDelay());

    return true;
}

bool VolumeInfo::OpenVolume(BasicIo::BioOpenMode mode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    AutoLock SetVolumelock( m_VolumeLock );
    if(!m_Volume)  
    {
        std::string dev_name(m_FormattedName);
        std::string mntPt(m_MountPoint);
        std::string sparsefile;
        bool isnewvirtualvolume = false;
        if(!IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(),sparsefile,isnewvirtualvolume)))
        {
            dev_name = sparsefile;
            mntPt = sparsefile;
        }
        m_Volume.reset(new cdp_volume_t(dev_name,isnewvirtualvolume));

        // Enable Retries on failures (required for fabric multipath device failures)
        ConfigureVxAgent & vxAgent = TheConfigurator->getVxAgent();
        m_Volume ->EnableRetry(CDPUtil::QuitRequested, vxAgent.getVolumeRetries(), vxAgent.getVolumeRetryDelay());

        m_Volume -> Open(mode);
        if (!m_Volume->Good()) {
            DebugPrintf(SV_LOG_ERROR, "%s volume open %s failed. err: %d\n",
                        FUNCTION_NAME, m_FormattedName.c_str(), m_Volume->LastError());
            return false;
        }
    }

    return ( m_Volume ->isOpen() && m_Volume -> Good());
}

cdp_volume_t::Ptr VolumeInfo::GetVolume()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (!OpenVolume(BasicIo::BioRWExisitng| BasicIo::BioShareAll | BasicIo::BioBinary )) {
        return cdp_volume_t::Ptr();
    }

    return m_Volume;
}

bool VolumeInfo::GetCDPV2Writer(CDPV2Writer::Ptr & ptr)
{
    if(!m_cdpv2writer)
    {
        bool diffsync = true;
        CDP_SETTINGS settings = TheConfigurator->getCdpSettings(m_Name);
        SV_ULONGLONG src_capacity = 0;
        if(!getSourceCapacity(*TheConfigurator, m_Name, src_capacity))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s failed in getSourceCapacity for %s.\n",
                FUNCTION_NAME, m_Name.c_str());
            return false;
        }



        CDPV2Writer * writer = new (nothrow)CDPV2Writer(m_Name, src_capacity, settings, diffsync, 0, m_azureagent_ptr,m_pVolumeSummariesCache);
        if(!writer)
            return false;

        m_cdpv2writer.reset(writer);
    }

    ptr = m_cdpv2writer;
    return m_cdpv2writer -> init();
}

bool VolumeInfo::MemoryAvailableForNextFile(SV_ULONG dataSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    AutoLock SetNumFilelock( m_MemoryUsageLock );
    SV_ULONG cbMaxMemoryUsagePerReplicaton = TheConfigurator ->getVxAgent().getMaxMemoryUsagePerReplication();
    if (m_MemoryUsage + dataSize <= cbMaxMemoryUsagePerReplicaton)      {
        m_MemoryUsage += dataSize;
        DebugPrintf(SV_LOG_DEBUG, "Volume:%s InMemory Usage: %lu MB\n", m_Name.c_str(), (m_MemoryUsage/(1024*1024)));
        return true;
    }

    return false;
}

void VolumeInfo::AdjustMemoryUsage(bool decrement, SV_ULONG dataSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    AutoLock SetNumFilelock( m_MemoryUsageLock );
    if(decrement)
        m_MemoryUsage -= dataSize;
    else
        m_MemoryUsage += dataSize;

    DebugPrintf(SV_LOG_DEBUG, "Volume:%s InMemory Usage: %lu MB\n", m_Name.c_str(), (m_MemoryUsage/(1024*1024)));
}

// --------------------------------------------------------------------------
// cleans out any stale diff data that may be in this volumes cache
// --------------------------------------------------------------------------
bool VolumeInfo::CleanCache()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // REFACTOR: merge with one in FastSync
    SVERROR sve = CleanupDirectory(m_CacheLocation.c_str(), "completed_ediffcompleted_diff_*.dat*");
    if (sve.failed()) {
        DebugPrintf(SV_LOG_ERROR, "VolumeInfo::CleanCache CleanupDirectory pattern (%s):  %s\n", m_CacheLocation.c_str(), sve.toString());
        return false;
    }
    return true;
}

bool VolumeInfo::EndQuasiState()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int status = 0;
    bool rv = sendEndQuasiStateRequest((*TheConfigurator), m_Name, status);
    if(!rv || (0 != status))
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                 << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                 << " Error Message: " 
                 << " failed to send end Quasi State Request, will try again on next invocation \n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        return false;
    }

    m_Quasi = false; 
    m_DiffSyncMode = true;

    return true;
}

void VolumeInfo::reset()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_DiffInfos.clear();
    m_TotalDiffDataSize = 0;
    assert(m_MemoryUsage == 0);
    m_MemoryUsage =0;
    m_Volume.reset();
}

SV_ULONGLONG VolumeInfo::GetTotalDiffSize()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    SV_ULONGLONG totalSize = 0;
    DiffInfo::DiffInfosOrderedEndTime_t::const_iterator iter = m_DiffInfos.begin();
    DiffInfo::DiffInfosOrderedEndTime_t::const_iterator end = m_DiffInfos.end();
    for( ;iter != end; iter++ )
        totalSize += (*iter)->Size();

    return totalSize;
}
