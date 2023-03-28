//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpdatabase.h
//
// Description: 
//

#ifndef CDPDATABASE__H
#define CDPDATABASE__H

#include <string>
#include <ace/Recursive_Thread_Mutex.h>

#include <boost/shared_ptr.hpp>

#include "cdpglobals.h"
#include "cdputil.h"

#include "cdpdatabaseimpl.h"
#include "retentionsettings.h"

class CDPDatabase
{
public:
    CDPDatabase(const std::string & dbname);
    virtual ~CDPDatabase();

    const std::string dbname() { return m_dbname ; }
    const std::string dbdir() { return m_dbdir ; }
    bool exists();

    virtual bool initialize();

    virtual bool getcdpsummary(CDPSummary & summary);
	virtual bool get_cdp_retention_diskusage_summary(CDPRetentionDiskUsage &);
    virtual bool getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end);
    virtual bool fetchseq_with_endtime_greq(SV_ULONGLONG & ts, SV_ULONGLONG & seq);

    virtual CDPDatabaseImpl::Ptr FetchEvents(CDPMarkersMatchingCndn  const & cndn);
    virtual CDPDatabaseImpl::Ptr FetchTimeRanges(CDPTimeRangeMatchingCndn const & cndn);
    virtual CDPDatabaseImpl::Ptr FetchDRTDs(CDPDRTDsMatchingCndn const & cndn);
    virtual CDPDatabaseImpl::Ptr FetchAppliedDRTDs();

    virtual bool UpdateVerifiedEvents(CDPMarkersMatchingCndn  const & cndn);
    virtual bool UpdateLockedBookmarks(const CDPMarkersMatchingCndn  & cndn,std::vector<CDPEvent> &cdpEvents,SV_USHORT &state);

    virtual bool enforce_policies(const CDP_SETTINGS & settings, 
        SV_ULONGLONG & start_ts, SV_ULONGLONG & end_ts,const std::string& volname);

    virtual bool delete_olderdata(const std::string& volname,
        SV_ULONGLONG & start_ts,
        const SV_ULONGLONG & timerange_to_free = HUNDREDS_OF_NANOSEC_IN_HOUR);

    virtual bool purge_retention(const std::string& volname);

    virtual bool get_retention_info(CDPInformation & info);
    virtual bool get_pending_updates(CDPInformation &info,const SV_ULONG & record_limit);
    virtual bool delete_pending_updates(CDPInformation &info);
	virtual bool delete_all_pending_updates();
    
    virtual bool delete_stalefiles();
    virtual bool delete_unusable_recoverypts();

protected:

    boost::shared_ptr<CDPDatabaseImpl> GetCDPDatabaseImpl();


private:

    CDPDatabase(CDPDatabase const & );
    CDPDatabase& operator=(CDPDatabase const & );

    std::string m_dbname;
    std::string m_dbdir;

    class {
    public:
        template<typename T>
        operator boost::shared_ptr<T>() { return boost::shared_ptr<T>(); }
    } nullPtr;

};

#endif
