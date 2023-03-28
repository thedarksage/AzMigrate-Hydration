//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3databaseimpl.h
//
// Description: 
//

#ifndef CDPV3DATABASEIMPL__H
#define CDPV3DATABASEIMPL__H

#include <string>
#include <boost/shared_ptr.hpp>
#include <sqlite3x.hpp>

#include "cdpdatabase.h"
#include "cdpv3tables.h"
#include "cdpv3metadatafile.h"
#include "cdpv3bitmap.h"
#include "cdpv3metadatareadahead.h"

class CDPV3DatabaseImpl : public CDPDatabaseImpl
{
public:

    CDPV3DatabaseImpl(const std::string & dbname);

    virtual ~CDPV3DatabaseImpl();

    virtual bool initialize();

    virtual bool getcdpsummary(CDPSummary & summary);
	virtual bool get_cdp_retention_diskusage_summary(CDPRetentionDiskUsage &);
    bool getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end);
    virtual bool fetchseq_with_endtime_greq(SV_ULONGLONG & ts, SV_ULONGLONG & seq);

    virtual bool enforce_policies(const CDP_SETTINGS & settings, 
        SV_ULONGLONG & start_ts, SV_ULONGLONG & end_ts,const std::string& volname);

    virtual bool delete_olderdata(const std::string& volname,
        SV_ULONGLONG & start_ts, 
        const SV_ULONGLONG & timerange_to_free = HUNDREDS_OF_NANOSEC_IN_HOUR);

    virtual bool purge_retention(const std::string& volname);

    virtual void setoperation(QUERYOP op, void const * context);

    virtual SVERROR read(CDPEvent & event);
    virtual SVERROR read(CDPTimeRange & timerange);
    virtual SVERROR read(cdp_rollback_drtd_t & drtd);

    virtual bool get_retention_info(CDPInformation & info);
    virtual bool get_pending_updates(CDPInformation &info,const SV_ULONG & record_limit);

    virtual bool delete_pending_updates(CDPInformation &info);
    virtual bool delete_all_pending_updates();
    virtual bool update_verified_events(const CDPMarkersMatchingCndn & cndn);
    virtual bool update_locked_bookmarks(const CDPMarkersMatchingCndn & cndn,std::vector<CDPEvent> &cdpEvents,SV_USHORT &newstatus);

    virtual bool  update_db(const CDPV3RecoveryRange & timerange,
        std::vector<CDPV3BookMark> & bookmarks, bool revoke = false);

	virtual bool validate();
	virtual bool delete_stalefiles();
    virtual bool delete_unusable_recoverypts();


private:

    bool   m_version_available_in_cache;
    double m_version;
    double m_revision;

    CDPDRTDsMatchingCndn const * m_drtdcndn;
    SV_ULONGLONG m_tsfc_inhrs;
    SV_ULONGLONG m_tslc_inhrs;
    SV_ULONGLONG m_metadatafiletime;

    bool m_readnextmetadatafile;
    bool m_readnextmetadata;
    SV_INT m_drtd_idx;

    cdpv3metadata_t m_cdpmetadata;
    cdpv3metadatafileptr_t  m_metadatafileptr;

    // to speed up rollback operation, metadata can be read ahead by multiple read ahead threads
    // m_readaheadptr - represents the read ahead threads
    // m_lastread_ts - represents latest metadata file scheduled for read ahead
    // m_isreadaheadcache_enabled - configurable based on which read ahead is enabled/disabled
    // m_readahead_threads - configurable representing number of read ahead threads
    // m_readahead_file_count - no. of files to read ahead
    // m_readahead_length - buffer size per metadata file

    bool m_isreadaheadcache_enabled;
    SV_UINT m_readahead_threads;
    SV_UINT m_readahead_file_count;
    SV_UINT m_readahead_length;
    cdpv3metadata_readahead_ptr_t m_readaheadptr;
    SV_ULONGLONG m_lastcache_ts;

    void fetch_readahead_configvalues();
    

private:



    void upgrade(double& revision);
    void UpgradeRev0();
    void UpgradeRev1();
    // private member routines required by getcdpsummary
    void getcdpsummary_nolock(CDPSummary & summary);
	void get_cdp_retention_diskusage_summary_nolock(CDPRetentionDiskUsage& );
    void getrecoveryrange_nolock(SV_ULONGLONG & start, SV_ULONGLONG & end);
    void get_filecount_spaceusage_nolock(SV_ULONGLONG & file_count, SV_ULONGLONG & spaceusage,SV_ULONGLONG & logicalsize  );
    void get_version_nolock(double &version, double & revision);
    void get_bookmarks_summary_nolock(EventSummary_t & summary);

    // private member routines required by fetchseq_with_endtime_greq
    void fetchseq_with_endtime_greq_nolock(SV_ULONGLONG & ts, SV_ULONGLONG & seq);

    void get_tsfc_greq_nolock(const SV_ULONGLONG &ts, SV_ULONGLONG & newtsfc);
    void delete_files_in_timerange(const SV_ULONGLONG & tsfc_hrs, 
        const SV_ULONGLONG & newtsfc_hrs,
        SV_LONGLONG & space_freed_in_current_run);

    // private member routines used by setoperation
    void form_event_query(const CDPMarkersMatchingCndn & cndn, std::string & querystring);
    void form_timerange_query(const CDPTimeRangeMatchingCndn & cndn, std::string & querystring);
    void form_drtd_query(const CDPDRTDsMatchingCndn & cndn, std::string & querystring);

    // private member routines used by get_retention_info
    void getcdptimeranges_nolock(CDPTimeRanges & ranges);
    void getcdpevents_nolock(CDPEvents & cdpevents);

    // private member routines used by 	get_pending_updates
    void getcdppendingtimeranges_nolock(CDPTimeRanges & ranges,const SV_ULONG & record_limit);
    void getcdppendingevents_nolock(CDPEvents & cdpevents,const SV_ULONG & record_limit);

    // private member routines used by 	delete_pending_updates
    void deletecdppendingtimeranges_nolock(CDPTimeRanges & ranges);
    void deletecdppendingevents_nolock(CDPEvents & events);

    void deleteallpendingtimeranges_nolock();
    void deleteallpendingevents_nolock();

    // private member routines used by update_verified_events
    void update_event_query_nolock(const CDPMarkersMatchingCndn & cndn);

    // private member routines used by update_locked_events
    void update_locked_bookmarks_query(const CDPMarkersMatchingCndn & cndn,SV_USHORT &state);
    void fetch_locked_bookmarks(const CDPMarkersMatchingCndn & cndn,std::vector<CDPEvent> &cdpEvents);

    // private member routines used by update_db
    void fetch_latest_timerange(CDPV3RecoveryRange & latestrange);
    void update_timerange_nolock(const CDPV3RecoveryRange & latestrange, 
        const CDPV3RecoveryRange &timerangeupdate);
    void insert_timerange_nolock(const CDPV3RecoveryRange &timerangeupdate);
    void insert_bookmarks_nolock(std::vector<CDPV3BookMark> & bookmarks);
    void revoke_bookmarks_nolock(std::vector<CDPV3BookMark> & bookmarks);

    // private member routines used by enforce policies
    bool enforce_sparse_policies(const CDP_SETTINGS & settings,
        const SV_ULONGLONG & tsfc_hr, const SV_ULONGLONG & tslc_hr,const std::string& volname);

    bool fetch_bookmarks_in_timerange(const SV_ULONGLONG & start_ts,
        const SV_ULONGLONG & end_ts,
        std::vector<CDPV3BookMark> & bookmarks);

    bool filter_bookmarks(const SV_ULONGLONG &tslc_hr, 
        const SV_UINT & tagcount,
        const SV_UINT & granularity,
        const SV_UINT & apptype,
        const std::vector<std::string> & userbookmarks,
        const std::vector<CDPV3BookMark> & bookmarks,
        std::vector<CDPV3BookMark> & bookmarks_to_preserve,
        std::vector<CDPV3BookMark> & bookmarks_to_delete);

    bool is_expired(const CDPV3BookMark & bkmk, const SV_ULONGLONG & tslc_hr);

    bool is_matching_timestamp(const SV_ULONGLONG & ts_to_match,
        const SV_ULONGLONG & seq_to_match,
        const std::vector<CDPV3BookMark> & bookmarks);

    bool is_matching_userbookmark(const std::string & bkmk_to_match,
        const std::vector<std::string> & userbookmarks);

    bool fetch_timestamps_to_preserve(
        const std::vector<CDPV3BookMark> & bookmarks_to_preserve,
        std::vector<CDPV3RecoveryRange> & timestamps_to_preserve);

    bool fetch_timeranges_in_db(const SV_ULONGLONG & start_ts,
        const SV_ULONGLONG & end_ts,
        std::vector<CDPV3RecoveryRange> & timeranges_in_db);

    bool is_already_coalesced(std::vector<CDPV3BookMark> & bookmarks_to_delete,
        std::vector<CDPV3RecoveryRange> & timeranges_in_db,
        std::vector<CDPV3RecoveryRange> & timestamps_to_preserve);

    bool fetch_cdptimestamps(const std::vector<CDPV3BookMark> & bkmks, 
        std::vector<cdp_timestamp_t> & timestamps_to_keep);

    bool delete_coalesced_bookmarks(std::vector<CDPV3BookMark> & bookmarks_to_delete);
    bool delete_coalesced_timeranges(std::vector<CDPV3RecoveryRange> & timeranges_in_db);
    bool add_coalesced_timestamps(std::vector<CDPV3RecoveryRange> & timestamps_to_preserve);

    bool delete_coalesced_datafiles(const SV_ULONGLONG & coalesce_start_ts, 
        const SV_ULONGLONG &coalesce_end_ts, 
        const std::vector<CDPV3BookMark> & bookmarks_to_keep,
        const std::vector<cdp_timestamp_t> &cdp_timestamps_to_keep);

    bool delete_coalesced_datafiles(const std::string & metadatafile,
		const std::string & logpath,
        const std::vector<CDPV3BookMark> & bookmarks_to_keep,
        const std::vector<cdp_timestamp_t> & timestamps_to_keep,
        const cdp_timestamp_t & cdp_coalesce_start_ts);

    bool parse_filename(const std::string & filename,
        unsigned long long & seq,
        cdp_timestamp_t & start_ts,
        cdp_timestamp_t & end_ts,
        bool & isoverlap);



    bool retain_bookmark(const UserTags & lhs, 
        const std::vector<CDPV3BookMark> & bookmarks_to_keep);

    void TrackExtraCoalescedFiles(std::string const & trackingPath, 
        std::string const & fileName);

    bool retain_filename(const std::string & filename, 
        const std::vector<cdp_timestamp_t> & timestamps_to_keep,
        const cdp_timestamp_t & cdp_coalesce_start_ts,
        bool & track_extra_coalesced_files);

    bool write_metadata(const std::string & metadatafilepath,
        std::vector<cdpv3metadata_t> & metadatas);

    bool write_metadata(const std::string & metadata_filepath,
        ACE_HANDLE &handle,
        SV_OFFSET_TYPE & endoffset,
        const cdpv3metadata_t & metadata);

	bool calculate_sparsesavings(SV_LONGLONG&);

	bool verify_datafiles_used_in_metadata_exist();
	bool verify_metadatafiles_for_all_datafiles_exist();
    bool verify_recovery_points();

    void detect_unusable_anypointrecovery_ranges(
        const std::vector<CDPV3RecoveryRange> & timeranges_to_check,
        std::set<CDPV3RecoveryRange> & unusableranges,
        std::set<std::string> & unexpected_deletion_logfiles);

    void detect_unusable_recoverypoints(
        const SV_ULONGLONG & recovery_start_ts,
        const SV_ULONGLONG & recovery_end_ts,
        const std::vector<CDPV3RecoveryRange> & timeranges_to_check,
        const std::vector<CDPV3BookMark> & bookmarks_to_check,
        std::set<CDPV3RecoveryRange> & unusablepoints, 
        std::set<CDPV3BookMark> & unusablebookmarks,
        std::set<std::string> &missing_datafiles);

    void find_affected_recoverypoints(const std::string & deleteddatfile,
        const std::vector<CDPV3RecoveryRange> & timeranges_to_check,
        const std::vector<CDPV3BookMark> & bookmarks_to_check,
        std::set<CDPV3RecoveryRange> & unusablepoints,
        std::set<CDPV3BookMark> &unusablebookmarks,
        std::set<std::string> &missing_datafiles);

    bool fetch_cdptimestamp(const CDPV3BookMark & bkmk, 
        cdp_timestamp_t & cdp_ts);

    bool fetch_cdptimestamp(const CDPV3RecoveryRange & recoverypt, 
        cdp_timestamp_t & cdp_ts);

    bool delete_recovery_pts(const std::set<CDPV3RecoveryRange> & unusablepoints,
        const std::set<CDPV3BookMark> &unusablebookmarks);

    bool log_deleted_unusablepoints(const std::set<CDPV3RecoveryRange> & unusablepoints,
        const std::set<CDPV3BookMark> &unusablebookmarks);

	bool get_listof_stalefiles(std::list<std::string> &stale_metadata_files);
	

    CDPV3DatabaseImpl(CDPV3DatabaseImpl const & );
    CDPV3DatabaseImpl& operator=(CDPV3DatabaseImpl const & );

};

#endif
