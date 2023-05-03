//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpdatabaseimpl.h
//
// Description: 
//

#ifndef CDPV1DATABASEIMPL__H
#define CDPV1DATABASEIMPL__H

#include <string>
#include <boost/shared_ptr.hpp>
#include <sqlite3x.hpp>

#include "cdpdatabase.h"
#include "cdpv1tables.h"



class CDPV1DatabaseImpl : public CDPDatabaseImpl
{
public:

    CDPV1DatabaseImpl(const std::string & dbname);

    virtual ~CDPV1DatabaseImpl();

    virtual bool initialize();

    virtual bool getcdpsummary(CDPSummary & summary);
	virtual bool get_cdp_retention_diskusage_summary(CDPRetentionDiskUsage &);
    virtual bool getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end);
    virtual bool fetchseq_with_endtime_greq(SV_ULONGLONG & ts, SV_ULONGLONG & seq);

    virtual bool enforce_policies(const CDP_SETTINGS & settings, 
        SV_ULONGLONG & start_ts, SV_ULONGLONG & end_ts,const std::string& volname);

    virtual bool delete_olderdata(const std::string& volname, 
        SV_ULONGLONG & start_ts,
        const SV_ULONGLONG & timerange_to_free = HUNDREDS_OF_NANOSEC_IN_HOUR);

    virtual bool purge_retention(const std::string& volname);

    virtual void setoperation(QUERYOP op, void const * context);
    virtual bool update_verified_events(const CDPMarkersMatchingCndn & cndn);
    virtual bool update_locked_bookmarks(const CDPMarkersMatchingCndn & cndn,std::vector<CDPEvent> &cdpEvents,SV_USHORT &state);

    virtual SVERROR read(CDPEvent & event);
    virtual SVERROR read(CDPTimeRange & timerange);
    virtual SVERROR read(cdp_rollback_drtd_t & drtd);

    virtual bool get_retention_info(CDPInformation & info);
    virtual bool get_pending_updates(CDPInformation &info,const SV_ULONG & record_limit);

    virtual bool delete_pending_updates(CDPInformation &info);
    virtual bool delete_all_pending_updates();
    virtual bool delete_stalefiles() { return true; }
    virtual bool delete_unusable_recoverypts() { return true; }

private:


    bool m_versionAvailableInCache;
    CDPVersion m_ver;

    // state information for providing drtds
    SV_ULONGLONG m_eventinodeno;
    CDPDRTDsMatchingCndn const * m_drtdcndn;
    bool m_inoderead;
    CDPV1Inode m_inode;
    CDPV1DiffInfos_t m_diffinfos;
    SV_INT m_diffinfoIndex;
    cdp_drtdv2s_riter_t m_drtdsIter;

    // state information for providong partial
    // transaction drtds
    CDPLastRetentionUpdate m_lastRetentionTxn;
    DiffPtr m_metadata;
    SV_LONG m_change;
    bool m_partialtxnread;


private:

    void getcdpsummary_nolock(CDPSummary & summary);
	void get_cdp_retention_diskusage_summary_nolock(CDPRetentionDiskUsage& );
    void getcdptimeranges_nolock(CDPTimeRanges & ranges);
    void getcdpevents_nolock(CDPEvents & events);
    bool upgrade();
    bool UpgradeRev2();
    bool UpgradeRev0();
    bool UpgradeRev1();

    void getcdppendingtimeranges_nolock(CDPTimeRanges & ranges,const SV_ULONG & record_limit);
    void getcdppendingevents_nolock(CDPEvents & events,const SV_ULONG & record_limit);

    void deletecdppendingtimeranges_nolock(CDPTimeRanges & ranges);
    void deletecdppendingevents_nolock(CDPEvents & events);

    void deleteallpendingtimeranges_nolock();
    void deleteallpendingevents_nolock();

    void ReadSuperBlock(CDPV1SuperBlock & superblock);
    void GetRevision(CDPVersion & ver);
    bool ReadInode(CDPV1Inode & inode, bool ReadDiff);

    void form_event_query(const CDPMarkersMatchingCndn & cndn, std::string & querystring);
    void FormUpdateVerifyEventQuery(const CDPMarkersMatchingCndn & cndn, std::string & querystring);
    void form_timerange_query(const CDPTimeRangeMatchingCndn & cndn, std::string & querystring);
    bool fetchVerifiedEvent(const CDPMarkersMatchingCndn & cndn,std::vector<CDPV1PendingEvent> & pendingEventList);

    SVERROR ReadDRTD(cdp_rollback_drtd_t & drtd);
    SVERROR ReadAppliedDRTD(cdp_rollback_drtd_t & drtd);
    bool FetchInodeNoForEvent(const std::string & eventvalue, 
        const SV_ULONGLONG & eventtime, SV_ULONGLONG & inodeNo );


    bool FetchOldestInode( CDPV1Inode & inode, bool ReadDiff);
    bool FetchLatestInode( CDPV1Inode & inode, bool ReadDiff);
    void DeleteInode(const CDPV1Inode & inode);
    void FetchEventsForInode(const CDPV1Inode & inode, CDPEvents &);
    void DeleteEvent(const CDPEvent&);
    void InsertPendingEvent(const CDPV1PendingEvent&);
    void UpdateSuperBlock(const CDPV1SuperBlock &);
    void AdjustTimeRangeTable(const CDPV1SuperBlock &);
    bool ReadEvent(CDPEvent &);
    bool ReadPendingEvent(CDPEvent &);
    bool ReadTimeRange(CDPTimeRange &);
    bool ReadPendingTimeRange(CDPTimeRange &);

    CDPV1DatabaseImpl(CDPV1DatabaseImpl const & );
    CDPV1DatabaseImpl& operator=(CDPV1DatabaseImpl const & );

};

#endif
