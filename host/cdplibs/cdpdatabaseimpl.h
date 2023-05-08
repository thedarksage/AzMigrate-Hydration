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

#ifndef CDPDATABASEIMPL__H
#define CDPDATABASEIMPL__H

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <sqlite3x.hpp>


#include "cdpglobals.h"
#include "cdpdrtd.h"


class CDPDatabaseImpl
{
public:

    typedef boost::shared_ptr<CDPDatabaseImpl> Ptr;

    CDPDatabaseImpl(const std::string & dbname);

    virtual ~CDPDatabaseImpl();

    const std::string dbname() { return m_dbname ; }
    const std::string dbdir() { return m_dbdir ; }
    const std::string lockname() { return m_dbname ; }

    virtual bool initialize() = 0;

    virtual void setoperation(QUERYOP op, void const * context) = 0;

    virtual bool update_verified_events(const CDPMarkersMatchingCndn & cndn) = 0;
    virtual bool update_locked_bookmarks(const CDPMarkersMatchingCndn & cndn,std::vector<CDPEvent> &cdpEvents,SV_USHORT &state) = 0;

    virtual bool getcdpsummary(CDPSummary & summary) = 0;
	virtual bool get_cdp_retention_diskusage_summary(CDPRetentionDiskUsage &) = 0;
    virtual bool getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end) = 0;
    virtual bool fetchseq_with_endtime_greq(SV_ULONGLONG & ts, SV_ULONGLONG & seq) = 0;

    virtual bool enforce_policies(const CDP_SETTINGS & settings, 
        SV_ULONGLONG & start_ts, SV_ULONGLONG & end_ts,const std::string& volname) = 0;

    virtual bool delete_olderdata(const std::string& volname,
        SV_ULONGLONG & start_ts,
        const SV_ULONGLONG & timerange_to_free = HUNDREDS_OF_NANOSEC_IN_HOUR) = 0;

    virtual bool purge_retention(const std::string& volname) = 0;

    virtual SVERROR read(CDPEvent & event) = 0;
    virtual SVERROR read(CDPTimeRange & timerange) = 0;
    virtual SVERROR read(cdp_rollback_drtd_t & drtd) = 0;

    virtual bool get_retention_info(CDPInformation & info) = 0;
    virtual bool get_pending_updates(CDPInformation &info,const SV_ULONG & record_limit) = 0;
    virtual bool delete_pending_updates(CDPInformation &info) =0;
    virtual bool delete_all_pending_updates() =0;
	virtual bool delete_stalefiles() = 0;
    virtual bool delete_unusable_recoverypts() = 0;
	
	cdp_io_stats get_io_stats();

protected:


    void connect();
    void disconnect();
    bool connected() { if(!m_con) return true; else return false; }
    void shutdown()
    {
        m_reader.close();
        disconnect();
    }

    void BeginTransaction();
    void CommitTransaction();


    void executestmts(std::vector<std::string> const & stmts);
    bool isempty(std::string const & table);
    bool good() { return good_status; }

	void update_io_stats(cdp_io_stats stats);

protected:


    CDPDatabaseImpl(CDPDatabaseImpl const & );
    CDPDatabaseImpl& operator=(CDPDatabaseImpl const & );

    std::string m_dbname;
    std::string m_dbdir;

    boost::shared_ptr<sqlite3x::sqlite3_connection> m_con;
    boost::shared_ptr<sqlite3x::sqlite3_transaction> m_trans;
    boost::shared_ptr<sqlite3x::sqlite3_command> m_cmd;
    sqlite3x::sqlite3_reader m_reader;

    QUERYOP m_op;
    void const * m_context;
    bool good_status;

	SV_UINT m_io_count;
	SV_ULONGLONG m_io_bytes;
};

#endif
