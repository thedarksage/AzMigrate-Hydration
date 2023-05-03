//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : volumeinfo.h
//
// Description:
//

#ifndef VOLUMEINFO__H
#define VOLUMEINFO__H

// #include <windows.h>
#include <string>
#include <list>

#include "diffinfo.h"
#include "cdpvolume.h"
#include "cdpglobals.h"
#include "dataprotectionutils.h"
#include "volumegroupsettings.h"
#include "cdpv2writer.h"

#include "azureagent.h"

time_t const CDP_UPDATE_INTERVAL_IN_SECONDS = 60 ; 

enum VSNAPOPERATION { MOUNT, UNMOUNT, REMOUNT };

class VolumeInfo {
public:
    typedef boost::shared_ptr<VolumeInfo> Ptr;
    typedef std::list<cdp_volume_t::Ptr> Volumes_t;
    typedef Volumes_t::iterator VolumeIterator;

    typedef int VolumeOpenMode;

    // must specify at least one of BIO_READ, BIO_WRITE, or BIO_READ_WRITE
    static VolumeOpenMode const VioRead            = (VolumeOpenMode)0x0001;                   // read only
    static VolumeOpenMode const VioWrite           = (VolumeOpenMode)0x0002;                   // write only
    static VolumeOpenMode const VioRW              = (VolumeOpenMode)(VioRead | VioWrite);     // read write (convienence)

    VolumeInfo(std::string const & name,
        std::string const & mountPoint,
        TRANSPORT_PROTOCOL protocol,
        bool secure,
        bool diffSync,
        bool visible,
        std::string const & diffLocation,
        std::string const & cacheLocation,
        bool resyncRequiredFlag = false, 
        VOLUME_SETTINGS::RESYNCREQ_CAUSE resyncRequiredCause = VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE,
        SV_ULONGLONG resyncRequiredTimestamp = 0,
        bool quasi = false,
        SV_ULONGLONG QuasiStartTime = 0,
        SV_ULONG     QuasiStartTimeSeq = 0,
        SV_ULONGLONG QuasiEndTime = 0,
        SV_ULONG     QuasiEndTimeSeq = 0,
        bool isAzureTarget = false,
        const VolumeSummaries_t *pVolumeSummariesCache = 0
        );

    bool OpenVolume(BasicIo::BioOpenMode);
    bool OpenVolumeExclusive();
    cdp_volume_t::Ptr GetVolume();
    bool Secure() { return m_Secure; }
    bool CleanCache();
    bool DiffSyncMode() { return m_DiffSyncMode; }

    //to retrieve resync required info & last updated time
    ACE_Time_Value LastTimeWhenCxUpdated(){return m_lastTimeWhenCxUpdated;}
    bool ResyncRequiredFlag(){return m_resyncRequiredFlag;}
    VOLUME_SETTINGS::RESYNCREQ_CAUSE ResyncRequiredCause(){return m_resyncRequiredCause;}
    SV_ULONGLONG ResyncRequiredTimestamp(){return m_resyncRequiredTimestamp;}

    //to set last updated time
    void LastTimeWhenCxUpdated(ACE_Time_Value &lastTimeWhenCxUpdated)
    { m_lastTimeWhenCxUpdated = lastTimeWhenCxUpdated;}

    TRANSPORT_PROTOCOL Protocol(){ return m_Protocol; }

    DiffInfo::DiffInfosOrderedEndTime_t & DiffInfos() { return m_DiffInfos; }

    void Add(DiffInfo::DiffInfoPtr diffInfo);
    void AddToTotalSize(unsigned long long size) { m_TotalDiffDataSize += size; }

    char const * Name() { return m_Name.c_str(); }

    std::string const & DiffLocation() { return m_DiffLocation; }
    std::string const & CacheLocation() { return m_CacheLocation; }
    std::string const & ResyncLocation() { return m_ResyncLocation; }

    bool OkToProcessDiffs() { return ((m_Quasi || m_DiffSyncMode) && !m_Visible && !m_Rollback); }
    SV_ULONGLONG QuasiStartTimeStamp() { return m_QuasiStartTime ; }
    SV_ULONGLONG QuasiEndTimeStamp() { return m_QuasiEndTime ; }
    SV_ULONG QuasiStartTimeStampSeq() { return m_QuasiStartTimeSeq ; }
    SV_ULONG QuasiEndTimeStampSeq() { return m_QuasiEndTimeSeq ; }
    bool InQuasiState() { return m_Quasi ;}
    bool EndQuasiState(); //{ m_Quasi = false; m_DiffSyncMode = true;}
    void reset();
    void AdjustMemoryUsage(bool decrement,SV_ULONG dataSize);
    bool MemoryAvailableForNextFile(SV_ULONG dataSize);

    void MarkForRollback() { m_Rollback = true; }

    void SetShouldCheckForCacheDirSpace();
    bool ShouldCheckForCacheDirSpace();

    SV_ULONGLONG GetTotalDiffSize();

    bool GetCDPV2Writer(CDPV2Writer::Ptr & ptr);

protected:


private:
    // no copying VolumeInfo
    VolumeInfo(VolumeInfo const & volumeInfo);
    VolumeInfo& operator=(VolumeInfo const & volumeInfo);

    bool m_Secure;
    bool m_DiffSyncMode;
    bool m_Visible;
    bool m_Rollback;
    bool m_Quasi;
    bool m_isAzureTarget;
    SV_ULONGLONG m_QuasiStartTime;
    SV_ULONG     m_QuasiStartTimeSeq;
    SV_ULONGLONG m_QuasiEndTime;
    SV_ULONG     m_QuasiEndTimeSeq;

    //to hold resync required info
    bool m_resyncRequiredFlag ;   
    VOLUME_SETTINGS::RESYNCREQ_CAUSE m_resyncRequiredCause;
    SV_ULONGLONG m_resyncRequiredTimestamp;
    //last time when Cx was sent updates
    ACE_Time_Value m_lastTimeWhenCxUpdated;

    TRANSPORT_PROTOCOL m_Protocol;

    std::string m_Name;
    std::string m_FormattedName;
    std::string m_DiffLocation;
    std::string m_CacheLocation;
    std::string m_ResyncLocation;
    std::string m_MountPoint;

    cdp_volume_t::Ptr m_Volume;

    unsigned long long m_TotalDiffDataSize;

    DiffInfo::DiffInfosOrderedEndTime_t m_DiffInfos;
    Lockable m_MemoryUsageLock;
    Lockable m_VolumeLock;
    SV_LONG m_MemoryUsage;


    // 0 - uninitialized
    // 1 - should check 
    // 2 - need not check, using file tal and cachedir and remotedir on same partition
    int m_bCheckCacheDirSpace;

    AzureAgentInterface::AzureAgent::ptr m_azureagent_ptr;
    CDPV2Writer::Ptr m_cdpv2writer;
    const VolumeSummaries_t *m_pVolumeSummariesCache;
};


#endif // ifndef VOLUMEINFO__H
