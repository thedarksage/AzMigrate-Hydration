//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license agreement with InMage.
//
// File:       cdpdatabase.cpp
//
// Description: 
//

#include "cdpdatabase.h"
#include "portablehelpers.h"

#include "cdpv1databaseimpl.h"
#include "cdpv3databaseimpl.h"

CDPDatabase::CDPDatabase(const std::string & dbname)
:m_dbname(dbname)
{
    m_dbname = dbname;
    m_dbdir = dirname_r(dbname.c_str());
}

CDPDatabase::~CDPDatabase()
{

}

//
// FUNCTION NAME :  GetCDPDatabaseImpl
//
// DESCRIPTION : Constructs and returns reference to appropriate CDPDatabaseImpl
//				 For V1 - CDPV1DatabaseImpl	
//				 For V3 - CDPV3DatabaseImpl
//
// INPUT PARAMETERS : none
//                   
//
// OUTPUT PARAMETERS : none
//
// ALGORITHM : 1. Compute the db version by comparing the dbname.
//			   2. If cdpv1.db, create an object of CDPV1DatabaseImpl
//			   3. If cdpv3.db, create an object of CDPV3DatabaseImpl
//			   4. Return reference to the created object.
//
// return value : CDPDatabaseImpl::Ptr
//
//
CDPDatabaseImpl::Ptr CDPDatabase::GetCDPDatabaseImpl()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());


    std::string dbpath = m_dbname;
    std::string dbname = basename_r(dbpath.c_str());
    if(dbname == CDPV1DBNAME)
    {
        return boost::shared_ptr<CDPV1DatabaseImpl>(new CDPV1DatabaseImpl(m_dbname));
    }
    else if(dbname == CDPV3DBNAME)
    {
        return boost::shared_ptr<CDPV3DatabaseImpl>(new CDPV3DatabaseImpl(m_dbname));
    }else
    {
        DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s unsupported dbpath:%s\n",
            FUNCTION_NAME, dbpath.c_str());
        return nullPtr;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

//
// FUNCTION NAME :  FetchEvents
//
// DESCRIPTION : Prepares the CDPDatabaseImpl object to be used for retrieving 
//				 bookmarks/events
//
// INPUT PARAMETERS : CDPMarkersMatchingCndn - condition to match events
//                   
//
// OUTPUT PARAMETERS : None
//
// ALGORITHM :	1. Create CDPDatbaseImpl object
//				2. Set the operation as CDPEVENT
//
// NOTES :      Once this function is called, CDPDatabaseImpl::read should be used to fetch individual events.
//
// return value : CDPDatabaseImpl::Ptr
//
//
CDPDatabaseImpl::Ptr CDPDatabase::FetchEvents( CDPMarkersMatchingCndn const & cndn )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        database->setoperation(CDPEVENT, &cndn);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return database;
}

//
// FUNCTION NAME :  UpdateVerifiedEvents
//
// DESCRIPTION : Prepares the CDPDatabaseImpl object to be used for update a event as verified 
//
// INPUT PARAMETERS : CDPMarkersMatchingCndn - condition to match events
//                   
//
// OUTPUT PARAMETERS : None
//
// ALGORITHM :	1. Create CDPDatbaseImpl object
//				2. calls update_verified_events
//
// NOTES :      
// return value : true on success otherwise false
//
//
bool CDPDatabase::UpdateVerifiedEvents(CDPMarkersMatchingCndn  const & cndn)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->update_verified_events(cndn);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  UpdateLockedBookmarks
//
// DESCRIPTION : Prepares the CDPDatabaseImpl object to be used for update a bookmark as locked/unlocked 
//
// INPUT PARAMETERS : CDPMarkersMatchingCndn - condition to match tag
//                   
//
// OUTPUT PARAMETERS : None
//
// ALGORITHM :	1. Create CDPDatbaseImpl object
//				2. calls update_locked_bookmarks
//
// NOTES :      
// return value : true on success otherwise false
//
//
bool CDPDatabase::UpdateLockedBookmarks(const CDPMarkersMatchingCndn  & cndn,std::vector<CDPEvent> &cdpEvents,SV_USHORT &newstatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());
    cdpEvents.clear(); 
    if(database)
        rv = database->update_locked_bookmarks(cndn,cdpEvents,newstatus);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPDatabase::exists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        ACE_stat statbuf = {0};
        // PR#10815: Long Path support
        if (0 != sv_stat(getLongPathName(m_dbname.c_str()).c_str(), &statbuf))
        {
            rv = false;
            break;
        }

        if ( (statbuf.st_mode & S_IFREG)!= S_IFREG )
        {
            rv = false;
            break;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

//
// FUNCTION NAME :  initialize
//
// DESCRIPTION : Initializes the appropriate CDPDatabaseImpl
//
//
// INPUT PARAMETERS : None
//                   
//
// OUTPUT PARAMETERS : None
//
//
// return value : true if initialized, else false
//
//
bool CDPDatabase::initialize()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->initialize();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  get_cdp_retention_diskusage_summary
//
// DESCRIPTION : Fetches summary information (CDPDiskUsage) associated with the database
//
//
// INPUT PARAMETERS : none
//                   
//
// OUTPUT PARAMETERS : CDPRetentionDiskUsage
//
//
// return value : true if success, else false
//
//

bool CDPDatabase:: get_cdp_retention_diskusage_summary(CDPRetentionDiskUsage & cdpdiskusage )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->get_cdp_retention_diskusage_summary(cdpdiskusage);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  getcdpsummary
//
// DESCRIPTION : Fetches summary information (CDPSummary) associated with the database
//
//
// INPUT PARAMETERS : none
//                   
//
// OUTPUT PARAMETERS : CDPSummary
//
//
// return value : 
//
//

bool CDPDatabase::getcdpsummary( CDPSummary & summary )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->getcdpsummary(summary);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}
//
// FUNCTION NAME :  getrecoveryrange
//
// DESCRIPTION : Fetches start and end time of retention associated with the database
//
//
// INPUT PARAMETERS : none
//                   
//
// OUTPUT PARAMETERS : start, end
//
//
// return value : 
//
//

bool CDPDatabase::getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->getrecoveryrange(start, end);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}
//
// FUNCTION NAME :  FetchTimeRanges
//
// DESCRIPTION : Prepares the CDPDatabaseImpl object to be used for retrieving 
//				 recovery ranges
//
// INPUT PARAMETERS : CDPTimeRangeMatchingCndn - condition to match recovery range
//                   
//
// OUTPUT PARAMETERS : None
//
// ALGORITHM :	1. Create CDPDatbaseImpl object
//				2. Set the operation as CDPTIMERANGE
//
// NOTES :      Once this function is called, CDPDatabaseImpl::read should be used to fetch individual time ranges.
//
// return value : CDPDatabaseImpl::Ptr
//
//
CDPDatabaseImpl::Ptr CDPDatabase::FetchTimeRanges( CDPTimeRangeMatchingCndn const & cndn )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        database->setoperation(CDPTIMERANGE, &cndn);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return database;
}
//
// FUNCTION NAME :  FetchDRTDs
//
// DESCRIPTION : Prepares the CDPDatabaseImpl object to be used for retrieving 
//				 DRTDs
//
// INPUT PARAMETERS : CDPDRTDsMatchingCndn - condition to stop retrieving further drtds 
//                   such as recoveryTime, sequenceId, event
//
// OUTPUT PARAMETERS : None
//
// ALGORITHM :	1. Create CDPDatbaseImpl object
//				2. Set the operation as CDPDRTD
//
// NOTES :      Once this function is called, CDPDatabaseImpl::read should be used to fetch individual DRTDs.
//
// return value : CDPDatabaseImpl::Ptr
//
//
CDPDatabaseImpl::Ptr CDPDatabase::FetchDRTDs( CDPDRTDsMatchingCndn const & cndn)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        database->setoperation(CDPDRTD, &cndn);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return database;
}
//
// FUNCTION NAME :  FetchAppliedDRTDs
//
// DESCRIPTION : Prepares the CDPDatabaseImpl object to be used for retrieving applied
//				 DRTDs
//
// INPUT PARAMETERS : AppliedInformation - information used to fetch the applied drtds from the retention file
//
//
// OUTPUT PARAMETERS : None
//
// ALGORITHM :	1. Create CDPDatbaseImpl object
//				2. Set the operation as CDPAPPLIEDDRTD
//
// NOTES :      Once this function is called, CDPDatabaseImpl::read should be used to fetch individual DRTDs.
//
// return value : CDPDatabaseImpl::Ptr
//
//
CDPDatabaseImpl::Ptr CDPDatabase::FetchAppliedDRTDs()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        database->setoperation(CDPAPPLIEDDRTD,NULL);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return database;

}
//
// FUNCTION NAME :  fetchseq_with_endtime_greq
//
// DESCRIPTION : Fetches the sequence number of the DRTD(change) whose timestamp matches or exceeds the provided timestamp
//
//
// INPUT PARAMETERS : timestamp
//                  
// 
// OUTPUT PARAMETERS : sequence number
//
// return value : true if success, else false
//
//
bool CDPDatabase::fetchseq_with_endtime_greq( SV_ULONGLONG & ts, SV_ULONGLONG & seq )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->fetchseq_with_endtime_greq(ts, seq);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  enforce_policies
//
// DESCRIPTION : enforce the specified cdp policies
//
//
// INPUT PARAMETERS : cdp policy
//                  
// 
// OUTPUT PARAMETERS : 
//
// return value : true if success, else false
//
//
bool CDPDatabase::enforce_policies(const CDP_SETTINGS & settings, 
                                   SV_ULONGLONG & start_ts, 
                                   SV_ULONGLONG & end_ts,const std::string& volname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->enforce_policies(settings,
        start_ts, end_ts,volname);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPDatabase::delete_olderdata(const std::string& volname,
                                   SV_ULONGLONG & start_ts, 
                                   const SV_ULONGLONG & timerange_to_free)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
    {
        rv = database->delete_olderdata(volname,start_ts, timerange_to_free);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


bool CDPDatabase::purge_retention(const std::string& volname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
    {
        rv = database->purge_retention(volname);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  get_retention_info
//
// DESCRIPTION : get updates that should be sent to cs
//
//
// INPUT PARAMETERS : none
//                  
// 
// OUTPUT PARAMETERS : CDPInformation
//
// return value : true if success, else false
//
//
bool CDPDatabase::get_retention_info(CDPInformation & info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->get_retention_info(info);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


//
// FUNCTION NAME :  get_pending_updates
//
// DESCRIPTION : get updates that should be sent to cs
//
//
// INPUT PARAMETERS : none
//                  
// 
// OUTPUT PARAMETERS : CDPInformation
//
// return value : true if success, else false
//
//
bool CDPDatabase::get_pending_updates(CDPInformation & info,const SV_ULONG & record_limit)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->get_pending_updates(info,record_limit);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  delete_pending_updates
//
// DESCRIPTION : delete entries that have been sent to cs
//
//
// INPUT PARAMETERS : none
//                  
// 
// OUTPUT PARAMETERS : CDPInformation
//
// return value : true if success, else false
//
//
bool CDPDatabase::delete_pending_updates(CDPInformation & info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->delete_pending_updates(info);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  delete_all_pending_updates
//
// DESCRIPTION : delete any pending bookmarks, timeranges information from retention
//               change made for SSE where this information does not need to
//               be sent
//
//
// INPUT PARAMETERS : none
//                  
// 
// OUTPUT PARAMETERS : CDPInformation
//
// return value : true if success, else false
//
//
bool CDPDatabase::delete_all_pending_updates()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->delete_all_pending_updates();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


bool CDPDatabase::delete_stalefiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->delete_stalefiles();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPDatabase::delete_unusable_recoverypts()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    CDPDatabaseImpl::Ptr database(GetCDPDatabaseImpl());

    if(database)
        rv = database->delete_unusable_recoverypts();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}