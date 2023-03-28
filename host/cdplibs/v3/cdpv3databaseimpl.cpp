//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3databaseimpl.cpp
//
// Description: 
//

#include "cdpv3databaseimpl.h"
#include "cdpv3metadatafile.h"
#include "portablehelpers.h"
#include "cdputil.h"
#include "cdplock.h"
#include "file.h"
#include "differentialfile.h"
#include "cdpplatform.h"
#include "inmageex.h"
#include "StreamRecords.h"
#include "localconfigurator.h"
#include "configwrapper.h"

#include <sstream>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "inmsafecapis.h"

using namespace sqlite3x;
using namespace std;

CDPV3DatabaseImpl::CDPV3DatabaseImpl(const std::string & dbname)
: CDPDatabaseImpl(dbname)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname: %s\n",FUNCTION_NAME, dbname.c_str());


    m_version_available_in_cache = false;
    m_version  = 0;
    m_revision = 0;

    m_drtdcndn = 0;
    m_tsfc_inhrs = 0;
    m_tslc_inhrs = 0;
    m_metadatafiletime = 0;
    m_readnextmetadatafile = false;
    m_readnextmetadata = false;
    m_drtd_idx = 0;
    m_metadatafileptr.reset();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname: %s\n",FUNCTION_NAME, dbname.c_str());
}

CDPV3DatabaseImpl::~CDPV3DatabaseImpl()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    m_metadatafileptr.reset();
    m_readaheadptr.reset(); 
    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV3DatabaseImpl::initialize()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();
        BeginTransaction();

        std::stringstream create_version_table ;
        std::stringstream create_recoveryranges_table ;
        std::stringstream create_bookmarks_table ;

        std::vector<std::string> stmts;

        // create version table
        create_version_table 
            << "create table " 
            << " if not exists "
            << CDPV3VERSIONTBLNAME
            << "(" ;

        for (int i = 0; i < CDPV3_NUM_VERSIONTBL_COLS ; ++i )
        {
            if(i)
                create_version_table << ",";
            create_version_table << CDPV3_VERSIONTBL_COLS[i];
        }
        create_version_table << ");" ;

        stmts.push_back(create_version_table.str());

        //create recoveryrange table
        create_recoveryranges_table 
            << "create table " 
            << " if not exists "
            << CDPV3RECOVERYRANGESTBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV3_NUM_RECOVERYRANGESTBL_COLS ; ++i )
        {
            if(i)
                create_recoveryranges_table << ",";
            create_recoveryranges_table << CDPV3_RECOVERYRANGESTBL_COLS[i];
        }
        create_recoveryranges_table << ");" ;

        stmts.push_back(create_recoveryranges_table.str());

        //create bookmarks table
        create_bookmarks_table 
            << "create table " 
            << " if not exists "
            << CDPV3BOOKMARKSTBLNAME
            << "(" ;

        for (int i = 0; i < CDPV3_NUM_BOOKMARKSTBL_COLS ; ++i )
        {
            if(i)
                create_bookmarks_table << ",";
            create_bookmarks_table << CDPV3_BOOKMARKSTBL_COLS[i];
        }
        create_bookmarks_table << ");" ;

        stmts.push_back(create_bookmarks_table.str());

        executestmts(stmts);

        stmts.clear();
        std::stringstream initialize_version_table;

        if(isempty(CDPV3VERSIONTBLNAME))
        {		
            initialize_version_table << "insert into " << CDPV3VERSIONTBLNAME 
                << " ( c_version, c_revision )"
                << " values(" 
                << CDPV3VER << ","
                << CDPV3VERCURRREV
                << ");" ;

            stmts.push_back(initialize_version_table.str());
            executestmts(stmts);
            stmts.clear();
        }
        else
        {
            //update the database if the version table have older version
            double version;
            double revision;
            m_version_available_in_cache = false;
            get_version_nolock(version, revision);
            if(revision < CDPV3VERCURRREV)
                upgrade(revision);
        }

        CommitTransaction();
        shutdown();
        rv = true;

    } catch (std::exception & ex)
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while initializing the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    return rv;
}

/*
* FUNCTION NAME : Upgrade
*
* DESCRIPTION : check for version to update tables
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
void CDPV3DatabaseImpl::upgrade(double &revision)
{
    if(revision == CDPV3VER3REV0)
        UpgradeRev0();
    else if(revision == CDPV3VER3REV1)
        UpgradeRev1();
}
/*
* FUNCTION NAME : UpgradeRev0
*
* DESCRIPTION : upgrade table in older version database
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
void CDPV3DatabaseImpl::UpgradeRev0()
{
    std::stringstream update_version_table ;
    std::stringstream upgrade_bookmark_table_imptag ;
    std::stringstream upgrade_bookmark_table_lifetime ;
    std::vector<std::string> stmts;

    upgrade_bookmark_table_imptag << " ALTER TABLE " << CDPV3BOOKMARKSTBLNAME << " ADD COLUMN " 
        << " c_lockstatus INTEGER DEFAULT 0";

    stmts.push_back(upgrade_bookmark_table_imptag.str());

    upgrade_bookmark_table_lifetime << " ALTER TABLE " << CDPV3BOOKMARKSTBLNAME << " ADD COLUMN " 
        << " c_lifetime INTEGER DEFAULT 0";

    stmts.push_back(upgrade_bookmark_table_lifetime.str());

    update_version_table << "update "<< CDPV3VERSIONTBLNAME 
        << " set " 
        << "c_version = "<< CDPV3VER <<","
        <<" c_revision = "<< CDPV3VERCURRREV << ";";

    stmts.push_back(update_version_table.str());

    executestmts(stmts);
}


/*
* FUNCTION NAME : UpgradeRev0
*
* DESCRIPTION : upgrade table in older version database
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
void CDPV3DatabaseImpl::UpgradeRev1()
{
    std::stringstream update_version_table ;
    std::stringstream upgrade_bookmark_table_lifetime ;
    std::vector<std::string> stmts;

    upgrade_bookmark_table_lifetime << " ALTER TABLE " << CDPV3BOOKMARKSTBLNAME << " ADD COLUMN " 
        << " c_lifetime INTEGER DEFAULT 0";

    stmts.push_back(upgrade_bookmark_table_lifetime.str());

    update_version_table << "update "<< CDPV3VERSIONTBLNAME 
        << " set " 
        << "c_version = "<< CDPV3VER <<","
        <<" c_revision = "<< CDPV3VERCURRREV << ";";

    stmts.push_back(update_version_table.str());

    executestmts(stmts);
}

//
// FUNCTION NAME :  getcdpsummary
//
// DESCRIPTION : Fetches the  summary information such as,
//				start/end timestamp, num of files, space, events
// This function is not completed, its a dummy
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : CDPSummary
//
// ALGORITHM : a. connects the database
//			   b. reads the superblock
//             c. populates the summary info.
//
// return value : true if success, else false
//
//
bool CDPV3DatabaseImpl::getcdpsummary(CDPSummary & summary)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    try
    {
        connect();

        getcdpsummary_nolock(summary);

        shutdown();
        rv = true;

    } catch( std::exception & ex )	
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while retrieving the summary from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

void CDPV3DatabaseImpl::getcdpsummary_nolock(CDPSummary & summary)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    // Summary:
    //   Recovery Range
    //   Avaiable Free space on volume
    //   Database Version,Revision
    //   Journal Type
    //   BookMark summary

    getrecoveryrange_nolock(summary.start_ts, summary.end_ts);
    GetDiskFreeCapacity(m_dbdir,summary.free_space);
    get_version_nolock(summary.version, summary.revision);
    summary.log_type = CDP_UNDO; 
    get_bookmarks_summary_nolock(summary.event_summary);

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

//
// FUNCTION NAME :  get_cdp_retention_diskusage_summary
//
// DESCRIPTION : Fetches the  summary information such as,
//				start/end timestamp, num of files, space on disk,size saved by compression or sparse policy
// This function is not completed, its a dummy
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : CDPRetentionDiskUsage
//
// ALGORITHM : a. connects the database
//			   b. reads the superblock
//             c. populates the CDPRetentionDiskUsage info.
//
// return value : true if success, else false
//
//
bool CDPV3DatabaseImpl::get_cdp_retention_diskusage_summary(CDPRetentionDiskUsage &cdpretentiondiskusage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    try
    {
        connect();

        get_cdp_retention_diskusage_summary_nolock(cdpretentiondiskusage);

        shutdown();
        rv = true;

    } catch( std::exception & ex )	
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while retrieving the summary from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


//
// FUNCTION NAME :  get_cdp_retention_diskusage_summary_nolock
//
// DESCRIPTION : Fetches the  summary information such as,
//				start/end timestamp, num of files, size on disk,space saved by compression or sparsepolicy 
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : CDPRetentionDiskUsage
//
//
//
void CDPV3DatabaseImpl::get_cdp_retention_diskusage_summary_nolock(CDPRetentionDiskUsage & retentionusage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
	SV_ULONGLONG logicalsize = 0;
	getrecoveryrange_nolock(retentionusage.start_ts, retentionusage.end_ts);
	get_filecount_spaceusage_nolock(retentionusage.num_files,retentionusage.size_on_disk,logicalsize);
	
	if(!calculate_sparsesavings(retentionusage.space_saved_by_sparsepolicy))
	{
		DebugPrintf(SV_LOG_ERROR," Failed to get the sparse savings .\n"); //this will be true for only cdpv3 file
	}

	if(logicalsize > retentionusage.size_on_disk)
		retentionusage.space_saved_by_compression = logicalsize - retentionusage.size_on_disk;
	else
		retentionusage.space_saved_by_compression = 0;
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}


void CDPV3DatabaseImpl::getrecoveryrange_nolock(SV_ULONGLONG & start, SV_ULONGLONG & end)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    do
    {
        if(isempty(CDPV3RECOVERYRANGESTBLNAME))
        {
            start = 0;
            end = 0;
            break;
        }


        std::stringstream query ;
        query << "select min(c_tsfc), max(c_tslc) from " <<  CDPV3RECOVERYRANGESTBLNAME
			<< " where c_cxupdatestatus != " << CS_DELETE_PENDING ;

        boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));
        sqlite3_reader reader = cmd -> executereader();

        if(!reader.read())
        {
            reader.close();
            throw INMAGE_EX("recoveryrange table is not yet initialized");
            return;
        }

        start = reader.getint64(0);
        end = reader.getint64(1);

        reader.close();


    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::get_filecount_spaceusage_nolock(SV_ULONGLONG & file_count, SV_ULONGLONG & spaceusage,SV_ULONGLONG & logicalsize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    do
    {
        if(!sv_get_filecount_spaceusage(m_dbdir.c_str(),CDPV3_FILE_PATTERN.c_str(),file_count,spaceusage,logicalsize))
        {
            throw INMAGE_EX("unable to get space usage for")(m_dbdir);
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::get_version_nolock(double & version, double & revision)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    do
    {

        if(m_version_available_in_cache)
        {
            version = m_version;
            revision = m_revision;
            break;
        }

        stringstream query ;
        query << "select * from " << CDPV3VERSIONTBLNAME  << " ; ";

        boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));
        sqlite3_reader reader= cmd -> executereader();

        if(!reader.read())
        {
            reader.close();
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Error Occured while while reading version information.\n"
                << "Database Name:" << m_dbname << "\n";

            throw INMAGE_EX(l_stdfatal.str().c_str());
            break;
        }

        m_version  = reader.getdouble(CDPV3_VERSIONTBL_C_VERSION_COL);
        m_revision = reader.getdouble(CDPV3_VERSIONTBL_C_REVISION_COL);
        version = m_version;
        revision = m_revision;
        m_version_available_in_cache = true;

        reader.close();

    } while ( 0 );

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::get_bookmarks_summary_nolock(EventSummary_t & summary)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    do
    {
        SV_EVENT_TYPE event_type;
        SV_ULONGLONG count;

        stringstream query ;
        query << "select c_apptype, count(c_apptype) from " 
            << CDPV3BOOKMARKSTBLNAME  
            << " where c_cxupdatestatus != " << CS_DELETE_PENDING
            << " group by  c_apptype;" ;

        boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));
        sqlite3_reader reader= cmd -> executereader();

        while (reader.read())
        {
            event_type = boost::lexical_cast<SV_USHORT>(reader.getint(0));
            count = boost::lexical_cast<SV_ULONGLONG>(reader.getint(1));
            summary.insert(make_pair(event_type,count));
        }

    } while ( 0 );

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV3DatabaseImpl::fetchseq_with_endtime_greq(SV_ULONGLONG & ts, SV_ULONGLONG & seq)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {

        connect();

        fetchseq_with_endtime_greq_nolock(ts,seq);

        shutdown();
        rv = true;

    }catch( std::exception & ex )	
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while retrieving metadata information from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

void CDPV3DatabaseImpl::fetchseq_with_endtime_greq_nolock(SV_ULONGLONG & ts, SV_ULONGLONG & seq)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    seq = 0;
    std::string metadatapathname = cdpv3metadatafile_t::getfilepath(ts,dbdir());
    cdpv3metadatafile_t mdfile(metadatapathname);
    if(!mdfile.get_lowestseq_gr_eq_ts(ts,seq))
    {
        throw INMAGE_EX("parse error")(metadatapathname);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV3DatabaseImpl::delete_olderdata(const std::string& volname,
                                         SV_ULONGLONG & start_ts,
                                         const SV_ULONGLONG & timerange_to_free)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do 
        {						
            CDPLock vol_lock(volname);
            SV_ULONGLONG newtsfc = 0;
            SV_ULONGLONG expected_newtsfc_hrs = 0;
            SV_ULONGLONG newtsfc_hrs = 0;
            SV_ULONGLONG end_ts =  0;

            getrecoveryrange_nolock(start_ts, end_ts);

            SV_ULONGLONG tsfc_hrs = 0;
            if(!CDPUtil::ToTimestampinHrs(start_ts, tsfc_hrs))
            {
                rv = false;
                break;
            }

            SV_ULONGLONG tslc_hrs = 0;
            if(!CDPUtil::ToTimestampinHrs(end_ts, tslc_hrs))
            {
                rv = false;
                break;
            }

            expected_newtsfc_hrs = tsfc_hrs + timerange_to_free; 
            if(tslc_hrs > tsfc_hrs)
            {

                get_tsfc_greq_nolock(expected_newtsfc_hrs,newtsfc);
                if(!CDPUtil::ToTimestampinHrs(newtsfc, newtsfc_hrs))
                {
                    rv = false;
                    break;
                }

            } else 
            {
				//Current hour purging is done by dataprotection. This is done to avoid lock contention
				//between cdpmgr and dataprotection. 
				//cdpmgr creates cdp_pruning file in the retention directory. Dataprotection upon seeing this
				//file will purge all the current hour data before proceeding with apply.


				std::string cdppruningfile = dbdir();
                cdppruningfile += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                cdppruningfile += "cdp_pruning";

                DebugPrintf(SV_LOG_DEBUG, "creating %s\n", cdppruningfile.c_str());
                ACE_HANDLE h =  ACE_OS::open(getLongPathName(cdppruningfile.c_str()).c_str(), 
                    O_RDWR | O_CREAT, FILE_SHARE_READ);

                if(ACE_INVALID_HANDLE != h) 
                {
                    ACE_OS::close(h);
                } else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s creation failed. errno: %d\n",
                        cdppruningfile.c_str(), ACE_OS::last_error());
                }

				//Remove the reserved file as we are going to purge all data. Dataprotection will recreate it.
				std::string reservedfilename = dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A + CDPUtil::CDPRESERVED_FILE;

				ACE_stat statdata={0};
				if(ACE_OS::stat(reservedfilename.c_str(),&statdata) == 0)
				{
					ACE_OS::unlink(getLongPathName(reservedfilename.c_str()).c_str());
				}

                rv = true;
                break;
                //newtsfc_hrs = expected_newtsfc_hrs;
            }


			if(IsVsnapDriverAvailable())
            {
                CDPUtil::UnmountVsnapsAssociatedWithInode(newtsfc_hrs,volname);
            }

            SV_LONGLONG space_freed_in_current_run = 0;
            delete_files_in_timerange(tsfc_hrs, newtsfc_hrs,space_freed_in_current_run);

            BeginTransaction();
            std::stringstream update_recoveryranges_table ;
            std::stringstream update_bookmarks_table ;

            std::vector<std::string> stmts;

            update_recoveryranges_table << " delete from " << CDPV3RECOVERYRANGESTBLNAME
                << " where c_tslc < " << newtsfc_hrs << ";" ;

            stmts.push_back(update_recoveryranges_table.str());

            update_bookmarks_table << " delete from " << CDPV3BOOKMARKSTBLNAME
                << " where c_timestamp < " << newtsfc_hrs << ";" ;

            stmts.push_back(update_bookmarks_table.str());

            executestmts(stmts);

            stmts.clear();
            CommitTransaction();

            getrecoveryrange_nolock(start_ts, end_ts);

        } while (0);

        shutdown();
    }
    catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Database:" << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";
        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


bool CDPV3DatabaseImpl::purge_retention(const std::string& volname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do 
        {						
            CDPLock vol_lock(volname);

            SV_ULONGLONG newtsfc_hrs = 0;
            SV_ULONGLONG start_ts = 0;
            SV_ULONGLONG end_ts =  0;

            getrecoveryrange_nolock(start_ts, end_ts);

            SV_ULONGLONG tsfc_hrs = 0;
            if(!CDPUtil::ToTimestampinHrs(start_ts, tsfc_hrs))
            {
                rv = false;
                break;
            }

            SV_ULONGLONG tslc_hrs = 0;
            if(!CDPUtil::ToTimestampinHrs(end_ts, tslc_hrs))
            {
                rv = false;
                break;
            }


            newtsfc_hrs = tslc_hrs + HUNDREDS_OF_NANOSEC_IN_HOUR; 

            if(IsVsnapDriverAvailable())
            {
                CDPUtil::UnmountVsnapsAssociatedWithInode(newtsfc_hrs,volname);
            }

            SV_LONGLONG space_freed_in_current_run = 0;
            delete_files_in_timerange(tsfc_hrs, newtsfc_hrs,space_freed_in_current_run);

            BeginTransaction();
            std::stringstream update_recoveryranges_table ;
            std::stringstream update_bookmarks_table ;

            std::vector<std::string> stmts;

            update_recoveryranges_table << " delete from " << CDPV3RECOVERYRANGESTBLNAME
                << " where c_tslc < " << newtsfc_hrs << ";" ;

            stmts.push_back(update_recoveryranges_table.str());

            update_bookmarks_table << " delete from " << CDPV3BOOKMARKSTBLNAME
                << " where c_timestamp < " << newtsfc_hrs << ";" ;

            stmts.push_back(update_bookmarks_table.str());

            executestmts(stmts);

            stmts.clear();
            CommitTransaction();


        } while (0);

        shutdown();
    }
    catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Database:" << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";
        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


bool CDPV3DatabaseImpl::enforce_policies(const CDP_SETTINGS & settings, 
                                         SV_ULONGLONG & lastrun_start_ts, 
                                         SV_ULONGLONG & lastrun_end_ts,const std::string& volname)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();


        do 
        {

            CDPLock vol_lock(volname);
            SV_ULONGLONG diskpolicy = settings.DiskSpace();
            SV_ULONGLONG timepolicy = settings.TimePolicy();

            SV_ULONGLONG start_ts = 0;
            SV_ULONGLONG end_ts = 0;
            SV_LONGLONG recoveryrange = 0;
            SV_ULONGLONG expected_newtsfc_hrs = 0;
            SV_ULONGLONG newtsfc = 0;
            SV_ULONGLONG newtsfc_hrs = 0;
            SV_ULONGLONG newrecoveryrange = 0;

            SV_ULONGLONG usedSpace = 0;
            SV_ULONGLONG file_count = 0;
            SV_ULONGLONG diskFreeSpace = 0;

            getrecoveryrange_nolock(start_ts, end_ts);

            SV_ULONGLONG tsfc_hrs = 0;
            if(!CDPUtil::ToTimestampinHrs(start_ts, tsfc_hrs))
            {
                rv = false;
                break;
            }

            SV_ULONGLONG tslc_hrs = 0;
            if(!CDPUtil::ToTimestampinHrs(end_ts, tslc_hrs))
            {
                rv = false;
                break;
            }
            recoveryrange = tslc_hrs - tsfc_hrs;

            SV_ULONGLONG lastrun_tslc_hrs = 0;
            if(!CDPUtil::ToTimestampinHrs(lastrun_end_ts,lastrun_tslc_hrs))
            {
                rv = false;
                break;
            }

            if(tslc_hrs != lastrun_tslc_hrs)
            {
                enforce_sparse_policies(settings,tsfc_hrs, tslc_hrs,volname);
            }

            lastrun_start_ts = start_ts;
            lastrun_end_ts = end_ts;

            SV_LONGLONG SpaceTobeFreed = 0;
            SV_LONGLONG timeRangeTobeFreed = 0;
            SV_LONGLONG freedSpace = 0;
            SV_LONGLONG freedTimeRange = 0;
            bool timeBasedPrunningDone = false;
            bool spaceBasedPrunningDone = false;
            SV_LONGLONG space_freed_in_current_run = 0;

            if(diskpolicy != 0)
            {
				SV_ULONGLONG  logicalsize = 0;
                get_filecount_spaceusage_nolock(file_count, usedSpace,logicalsize);
                if(usedSpace > diskpolicy)
                {
                    SpaceTobeFreed = usedSpace - diskpolicy;
                } else
                {
                    spaceBasedPrunningDone = true;
                }

            } else
            {
                spaceBasedPrunningDone = true;
            }


            if((timepolicy != 0) && (recoveryrange > timepolicy))
            {
                timeRangeTobeFreed = recoveryrange -timepolicy;
            } else
            {
                timeBasedPrunningDone = true;
            }

            if(!SpaceTobeFreed && !timeRangeTobeFreed )
            {
                rv = true;
                break;
            }

            while (true)
            {

                expected_newtsfc_hrs = tsfc_hrs + HUNDREDS_OF_NANOSEC_IN_HOUR; 
                if(tslc_hrs  > tsfc_hrs)
                {
                    get_tsfc_greq_nolock(expected_newtsfc_hrs,newtsfc);

                    if(!CDPUtil::ToTimestampinHrs(newtsfc, newtsfc_hrs))
                    {
                        rv = false;
                        break;
                    }
                } else
                {
                    //we will keep the latest hour data
                    //if retention space is insufficient 
                    //then it will be deleted in retention purging due to insufficient space
                    rv = true;
                    break;
                }

                if(tslc_hrs > newtsfc_hrs)
                    newrecoveryrange = tslc_hrs - newtsfc_hrs;
                else
                    newrecoveryrange = 0;

                if((timepolicy != 0) && (newrecoveryrange < timepolicy))
                {
                    timeBasedPrunningDone = true;
                }

                if(freedTimeRange >= timeRangeTobeFreed)
                {
                    timeBasedPrunningDone = true;
                }
                if(freedSpace >= SpaceTobeFreed)
                {
                    spaceBasedPrunningDone = true;
                }

                if((spaceBasedPrunningDone && timeBasedPrunningDone ))
                {
                    rv = true;
                    break;
                }

				if(IsVsnapDriverAvailable())
                {
                    CDPUtil::UnmountVsnapsAssociatedWithInode(newtsfc_hrs,volname);
                }
                delete_files_in_timerange(tsfc_hrs, newtsfc_hrs,space_freed_in_current_run);

                freedTimeRange += HUNDREDS_OF_NANOSEC_IN_HOUR;
                tsfc_hrs = newtsfc_hrs;
                recoveryrange = newrecoveryrange;

                if(!spaceBasedPrunningDone)
                {
                    freedSpace += space_freed_in_current_run;
                }

                BeginTransaction();
                std::stringstream update_recoveryranges_table ;
                std::stringstream update_bookmarks_table ;

                std::vector<std::string> stmts;

                update_recoveryranges_table << " delete from " << CDPV3RECOVERYRANGESTBLNAME
                    << " where c_tslc < " << tsfc_hrs << ";" ;

                stmts.push_back(update_recoveryranges_table.str());

                update_bookmarks_table << " delete from " << CDPV3BOOKMARKSTBLNAME
                    << " where c_timestamp < " << tsfc_hrs << ";" ;

                stmts.push_back(update_bookmarks_table.str());

                executestmts(stmts);

                stmts.clear();
                CommitTransaction();
            }

            getrecoveryrange_nolock(lastrun_start_ts, lastrun_end_ts);

        } while (0);

        shutdown();
    }
    catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Database:" << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";
        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

void CDPV3DatabaseImpl::get_tsfc_greq_nolock(const SV_ULONGLONG &ts, 
                                             SV_ULONGLONG & newtsfc)
{	
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    do
    {
        if(isempty(CDPV3RECOVERYRANGESTBLNAME))
        {
            newtsfc = 0;
            break;
        }


        std::stringstream query ;
        query << "select c_tsfc from " <<  CDPV3RECOVERYRANGESTBLNAME
            << " where c_tsfc >= " << ts 
			<< " and c_cxupdatestatus != " << CS_DELETE_PENDING
            << " order by c_tsfc ASC limit 1";

        boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));
        sqlite3_reader reader = cmd -> executereader();

        if(!reader.read())
        {
            reader.close();
            newtsfc = 0;
            break;
        }

        newtsfc = reader.getint64(0);
        reader.close();
    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::delete_files_in_timerange(const SV_ULONGLONG & tsfc_hrs, 
                                                  const SV_ULONGLONG & newtsfc_hrs,
                                                  SV_LONGLONG & space_freed_in_current_run)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s " ULLSPEC "  " ULLSPEC "\n",
        FUNCTION_NAME, m_dbname.c_str(), tsfc_hrs, newtsfc_hrs);

    do
    {

        SV_ULONGLONG ts = tsfc_hrs;
        SV_ULONGLONG filesize = 0;
        SVERROR sv = SVS_OK;
        space_freed_in_current_run = 0;
        cdpv3metadata_t metadata;

        for( ; ts < newtsfc_hrs; ts += HUNDREDS_OF_NANOSEC_IN_HOUR)
        {
            std::string metadatafilepath = cdpv3metadatafile_t::getfilepath(ts, dbdir());
            std::string logfilepath = cdpv3metadatafile_t::getlogpath(ts, dbdir());

            cdpv3metadatafile_t mdfile(metadatafilepath);

            while((sv = mdfile.read_metadata_asc(metadata)) == SVS_OK)
            {
                for(unsigned int file_idx =0; file_idx < metadata.m_filenames.size() ;++file_idx)
                {
                    std::string datafilename = dbdir();
                    datafilename += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                    datafilename += metadata.m_filenames[file_idx];
                    filesize = File::GetSizeOnDisk(datafilename);

                    if((0 != ACE_OS::unlink(getLongPathName(datafilename.c_str()).c_str())) 
                        && (ACE_OS::last_error() != ENOENT))
                    {
                        stringstream l_stdfatal;
                        l_stdfatal << "Error detected  " << "in Function:" 
                            << FUNCTION_NAME << " @ Line:" << LINE_NO
                            << " FILE:" << FILE_NAME << "\n\n"
                            << " Error occured on unlink of data file: " 
                            << datafilename << "\n"
                            << " Error Code : " << Error::Code() << "\n"
                            << " Error Message:" << Error::Msg() << "\n"
                            << " Please delete the file manually\n";

                        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());		
                    }

                    space_freed_in_current_run  += filesize;
                }
                metadata.clear();
            }

            mdfile.close();

            if(sv.failed())
            {
                stringstream l_stderr;
                DebugPrintf(SV_LOG_ERROR, 
                    "FUNCTION:%s unable to parse %s. Please delete the file manually.\n", 
                    FUNCTION_NAME, metadatafilepath.c_str());
            }

            if(sv == SVS_FALSE)
            {
                filesize = File::GetSizeOnDisk(metadatafilepath);
                // PR#10815: Long Path support
                if((0 != ACE_OS::unlink(getLongPathName(metadatafilepath.c_str()).c_str()))
                    && (ACE_OS::last_error() != ENOENT))
                {
                    stringstream l_stdfatal;
                    l_stdfatal << "Error detected  " << "in Function:" 
                        << FUNCTION_NAME << " @ Line:" << LINE_NO
                        << " FILE:" << FILE_NAME << "\n\n"
                        << " Error occured on unlink of metadata file: " 
                        << metadatafilepath << "\n"
                        << " Error Code : " << Error::Code() << "\n"
                        << " Error Message:" << Error::Msg() << "\n"
                        << " Please delete the file manually\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());			
                }
                space_freed_in_current_run  += filesize;


                filesize = File::GetSizeOnDisk(logfilepath);
                // PR#10815: Long Path support
                if((0 != ACE_OS::unlink(getLongPathName(logfilepath.c_str()).c_str()))
                    && (ACE_OS::last_error() != ENOENT))
                {
                    stringstream l_stdfatal;
                    l_stdfatal << "Error detected  " << "in Function:" 
                        << FUNCTION_NAME << " @ Line:" << LINE_NO
                        << " FILE:" << FILE_NAME << "\n\n"
                        << " Error occured on unlink of log file: " 
                        << logfilepath << "\n"
                        << " Error Code : " << Error::Code() << "\n"
                        << " Error Message:" << Error::Msg() << "\n"
                        << " Please delete the file manually\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());			
                }
                space_freed_in_current_run  += filesize;
            }

        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::setoperation(QUERYOP op, void const * context)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    try
    {
        std::string query;

        m_op = op;
        m_context = context;
        good_status = true;
        if(CDPEVENT == op)
        {
            CDPMarkersMatchingCndn const * event_cndn = 
                reinterpret_cast<CDPMarkersMatchingCndn const *>(context);
            form_event_query(*event_cndn, query);
        } 
        else if(CDPTIMERANGE == op)
        {
            CDPTimeRangeMatchingCndn const * timerange_cndn = 
                reinterpret_cast<CDPTimeRangeMatchingCndn const *>(context);
            form_timerange_query(*timerange_cndn, query);
        }
        else if (CDPDRTD == op)
        {
            m_drtdcndn = reinterpret_cast<CDPDRTDsMatchingCndn const *>(m_context);
            m_tsfc_inhrs = 0;
            m_tslc_inhrs = 0;
            m_metadatafiletime = 0;
            m_readnextmetadatafile = true;
            m_readnextmetadata = false;
            m_drtd_idx = 0;
            m_metadatafileptr.reset();
        }
        else if (CDPAPPLIEDDRTD == op)
        {
            //TODO: pending implementation
            // for now, we will assume there is no
            // pending write and always return as SVS_FALSE


        } else
        {
            good_status = false;
        }

        if(good_status  && op != CDPAPPLIEDDRTD && op != CDPDRTD )
        {
            connect();
            sqlite3_command * cmd = new sqlite3_command(*(m_con),query);
            m_cmd.reset(cmd);
            m_reader = m_cmd -> executereader();
        }

    }catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Database:" << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        shutdown();
        good_status = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}


void CDPV3DatabaseImpl::form_event_query(const CDPMarkersMatchingCndn & cndn, std::string & querystring)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    std::vector<std::string> clause;

    if(cndn.type())
    {
        std::stringstream tmp;
        tmp << "c_apptype =" << cndn.type();
        clause.push_back(tmp.str());
    }

    if(!cndn.value().empty())
    {
        std::stringstream tmp;
        tmp << "c_value =" << "\"" << cndn.value() << "\"" ;
        clause.push_back(tmp.str());
    }

    if(!cndn.identifier().empty())
    {
        std::stringstream tmp;
        tmp << "c_identifier =" << "\"" << cndn.identifier() << "\"";
        clause.push_back(tmp.str());
    }

    if(cndn.atTime())
    {
        std::stringstream tmp;
        tmp << "c_timestamp =" << cndn.atTime();
        clause.push_back(tmp.str());
    }

    if(cndn.beforeTime())
    {
        std::stringstream tmp;
        tmp << "c_timestamp <" << cndn.beforeTime();
        clause.push_back(tmp.str());
    }

    if(cndn.afterTime())
    {
        std::stringstream tmp;
        tmp << "c_timestamp >" << cndn.afterTime();
        clause.push_back(tmp.str());
    }

    if(cndn.mode() != ACCURACYANY)
    {
        std::stringstream tmp;
        tmp << "c_accuracy =" << cndn.mode();
        clause.push_back(tmp.str());
    }

    if(cndn.verified() != BOTH_VERIFIED_NONVERIFIED_EVENT)
    {
        std::stringstream tmp;
        tmp << "c_verified =" << cndn.verified();
        clause.push_back(tmp.str());
    }

    if((cndn.lockstatus() != BOOKMARK_STATE_BOTHLOCKED_UNLOCKED) 
        && (cndn.lockstatus() != BOOKMARK_STATE_LOCKED))
    {
        std::stringstream tmp;
        tmp << "c_lockstatus =" << cndn.lockstatus();
        clause.push_back(tmp.str());
    }

    if(cndn.lockstatus() == BOOKMARK_STATE_LOCKED)
    {
        std::stringstream tmp;               
        tmp << "c_lockstatus !=" << BOOKMARK_STATE_UNLOCKED;
        clause.push_back(tmp.str());
    }
    std::stringstream tmp;
    tmp << " c_cxupdatestatus != " << CS_DELETE_PENDING;
    clause.push_back(tmp.str());

    std::stringstream query ;
    query << "select * from " << CDPV3BOOKMARKSTBLNAME ;

    std::vector<std::string>::iterator iter     = clause.begin();
    std::vector<std::string>::iterator iter_end = clause.end();
    if(iter != iter_end)
    {
        query << " where " << *iter ;
        ++iter;
    }

    for ( ; iter != iter_end ; ++iter )
    {
        query << " and " << *iter ;
    }

    std::string sortcol = cndn.sortby();
    if ( sortcol == "time" )
    {
        query << " order by c_timestamp";
    }
    else if (sortcol == "app" )
    {
        query << " order by c_apptype";
    }
    else if (sortcol == "event" )
    {
        query << " order by c_value";
    }
    else if (sortcol == "accuracy")
    {
        query << " order by c_accuracy";
    }
    else // default
    {
        query << " order by c_timestamp";
    }

    std::string sortorder  = cndn.sortorder();
    if(sortorder == "default")
    {
        if ( cndn.afterTime() || cndn.atTime())
        {
            query  << " ASC ";
        }
        else
        {
            query << " DESC ";
        }
    } else
    {
        query << " " << sortorder << " " ;
    }

    if(cndn.limit())
    {
        query << " limit " << cndn.limit();
    }

    querystring = query.str();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}


void CDPV3DatabaseImpl::form_timerange_query(const CDPTimeRangeMatchingCndn & cndn, std::string & querystring)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    std::vector<std::string> clause;

    if(cndn.mode() != ACCURACYANY)
    {
        std::stringstream tmp;
        tmp << "c_accuracy =" << cndn.mode();
        clause.push_back(tmp.str());
    }

    std::stringstream tmp;
    tmp << " c_cxupdatestatus != " << CS_DELETE_PENDING;
    clause.push_back(tmp.str());

    std::stringstream query;
    query << "select * from " << CDPV3RECOVERYRANGESTBLNAME ;

    std::vector<std::string>::iterator iter = clause.begin();
    std::vector<std::string>::iterator iter_end = clause.end();

    if(iter != iter_end)
    {
        query << " where " << *iter;
        ++iter;
    }

    for( ; iter != iter_end; ++iter )
    {
        query << " and " << *iter;
    }

    std::string sortorder = cndn.sortorder();
    if(sortorder == "default")
    {
        sortorder = " DESC";
    }
    else
    {
        sortorder = " ASC";
    }

    std::string sortcol = cndn.sortby();
    if(sortcol == "accuracy")
    {
        query << " order by c_accuracy" << sortorder ;
    }
    else
    {
        query << " order by c_tsfc" << sortorder << ", c_tsfcseq" << sortorder;
    }


    if(cndn.limit())
    {
        query << " limit " << cndn.limit();
    }

    querystring = query.str();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}


SVERROR CDPV3DatabaseImpl::read(CDPEvent & cdpevent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    SVERROR hr = SVS_OK;

    try
    {
        do 
        {

            if(!good())
            {
                DebugPrintf(SV_LOG_ERROR, "Bad stream status. function:%s dbname:%s\n",
                    FUNCTION_NAME, m_dbname.c_str());
                hr = SVE_FAIL;
                break;
            }

            // all events matching condition are provided
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                break;
            }

            cdpevent.c_eventno = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_ENTRYNO_COL);
            cdpevent.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_APPTYPE_COL));
            cdpevent.c_eventvalue = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_VALUE_COL); 
            cdpevent.c_eventmode = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_ACCURACY_COL));
            cdpevent.c_eventtime = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_TIMESTAMP_COL);
            cdpevent.c_eventseq = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_SEQUENCE_COL);
            cdpevent.c_identifier = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_IDENTIFIER_COL);
            cdpevent.c_verified = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_VERIFIED_COL));
            cdpevent.c_comment = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_COMMENT_COL);
            SV_UINT status = m_reader.getint(CDPV3_BOOKMARKSTBL_C_CXUPDATESTATUS_COL);
            if(status == CS_DELETE_PENDING)
            {
                cdpevent.c_status = CDP_BKMK_DELETE_REQUEST;
            }else
            {
                if(cdpevent.c_verified == VERIFIEDEVENT)
                {
                    cdpevent.c_status = CDP_BKMK_UPDATE_REQUEST;
                } else 
                {
                    cdpevent.c_status = CDP_BKMK_INSERT_REQUEST;
                }
            }
            //this is added as part of upgrade call
            //as new field c_lockstatus is added into database so once upgrade is done and service is in stop state 
            //then all cdpcli command should work

            double version;
            double revision;
            get_version_nolock(version, revision);
            if(revision >= CDPV3VER3REV1)
            {
                cdpevent.c_lockstatus = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_LOCKED_COL ));
            }
            else
            {
                cdpevent.c_lockstatus = BOOKMARK_STATE_UNLOCKED; 
            }

            if(revision >= CDPV3VER3REV2)
            {
                cdpevent.c_lifetime = static_cast<SV_ULONGLONG> (m_reader.getint64(CDPV3_BOOKMARKSTBL_C_LIFETIME_COL ));
            }
            else
            {
                cdpevent.c_lifetime = INM_BOOKMARK_LIFETIME_NOTSPECIFIED; 
            }


            hr = SVS_OK;

        } while (0);

        // disconnect on failure or 
        // on finished sending all records
        if(hr != SVS_OK)
        {
            shutdown();
        }

    } catch(exception &ex){
        std::stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured while reading bookmarks\n"
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV3BOOKMARKSTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*(m_con)).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

        shutdown();
        good_status = false;
        hr = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
    return hr;
}


SVERROR CDPV3DatabaseImpl::read(CDPTimeRange & timerange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    SVERROR hr = SVS_OK;

    try
    {
        do 
        {
            if(!good())
            {
                DebugPrintf(SV_LOG_ERROR, "Bad stream status function:%s dbname:%s\n",
                    FUNCTION_NAME, m_dbname.c_str());
                hr = SVE_FAIL;
                break;
            }

            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                break;
            }

            timerange.c_entryno = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_ENTRYNO_COL);
            timerange.c_starttime = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFC_COL);
            timerange.c_endtime = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLC_COL);
            timerange.c_startseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFCSEQ_COL);
            timerange.c_endseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLCSEQ_COL);
            timerange.c_granularity = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_GRANULARITY_COL));
            timerange.c_mode = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_ACCURACY_COL));
            SV_UINT status = m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_CXUPDATESTATUS_COL);
            if(status == CS_DELETE_PENDING)
            {
                timerange.c_action = CDP_TIMERANGE_DELETE_REQUEST;
            }else
            {
                timerange.c_action = CDP_TIMERANGE_INSERT_REQUEST;
            }


            hr = SVS_OK;

        } while (0);

        // disconnect on failure or 
        // on finished sending all records
        if(hr != SVS_OK)
        {
            shutdown();
        }

    } catch(exception &ex){
        std::stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured while reading timeranges\n"
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV3RECOVERYRANGESTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*(m_con)).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        shutdown();
        good_status = false;
        hr = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
    return hr;
}

SVERROR CDPV3DatabaseImpl::read(cdp_rollback_drtd_t & rollback_drtd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    SVERROR hr = SVS_OK;
	cdp_io_stats_t stats;

    try
    {
        do 
        {
            //__asm int 3;
            // TODO: pending implementation
            if(m_op == CDPAPPLIEDDRTD)
            {
                hr = SVS_FALSE;
                break;
            }

            if(!good())
            {
                hr = SVE_FAIL;
                break;
            }

            if(0 == m_tsfc_inhrs)
            {
                SV_ULONGLONG tsfc, tslc;
                if(!getrecoveryrange(tsfc, tslc))
                {
                    good_status = false;
                    hr = SVE_FAIL;
                    break;
                }

                if(!CDPUtil::ToTimestampinHrs(tsfc, m_tsfc_inhrs))
                {
                    good_status = false;
                    hr = SVE_FAIL;
                    break;
                }

                if(!CDPUtil::ToTimestampinHrs(tslc, m_tslc_inhrs))
                {
                    good_status = false;
                    hr = SVE_FAIL;
                    break;
                }


                if(m_drtdcndn-> fromTime())
                {
                    if(!CDPUtil::ToTimestampinHrs(m_drtdcndn-> fromTime(), m_metadatafiletime))
                    {
                        good_status = false;
                        hr = SVE_FAIL;
                        break;
                    }
                }else
                {

                    m_metadatafiletime = m_tslc_inhrs;
                    m_lastcache_ts = 0;
                    fetch_readahead_configvalues();
	
                    // TODO: what if new or thread creation fails
                    if(m_isreadaheadcache_enabled && !m_readaheadptr) 
                    {
                        m_readaheadptr.reset(new cdpv3metadata_readahead_t(m_readahead_threads,m_readahead_length));
                    }
                }
            }

            bool readahead = m_isreadaheadcache_enabled; 
            while(true)
            {

                //if the flag m_readnextmetadatafile is set to true then we will fetch the next metadata file
                if(m_readnextmetadatafile)
                {
                    if(m_metadatafiletime < m_tsfc_inhrs)
                    {
                        hr = SVS_FALSE;
                        break;
                    }

                    m_readnextmetadatafile = false;
                    m_readnextmetadata = true;

                    // to speed up rollback operation, metadata can be read ahead by multiple read ahead threads
                    // m_readaheadptr - represents the read ahead thread
                    // m_lastcache_ts - represents latest metadata file scheduled for read ahead
                    // m_isreadaheadcache_enabled - configurable based on which read ahead is enabled/disabled
                    // m_readahead_threads - configurable representing number of read ahead threads
                    // m_readahead_file_count - no. of files to read ahead
		
                    if(m_isreadaheadcache_enabled)
                    {
                        SV_ULONGLONG metadata_ts_to_cache = m_metadatafiletime;
                        int readahead_file_count = m_readahead_file_count;
                        ACE_stat statbuf;

                        // in the first round, we send m_readahead_file_count for caching
                        // from next round onwards, only 1 file ahead for caching 
                        // this ensures we do not maintaing more than m_readahead_file_count 
                        // no of read ahead file cache
                        if(m_lastcache_ts)
                        {
                            readahead_file_count = 1;
                            metadata_ts_to_cache = m_lastcache_ts;
                        }

                        while(readahead && readahead_file_count && (metadata_ts_to_cache >= m_tsfc_inhrs))
                        {
                            std::string metadatapathname = 
                                cdpv3metadatafile_t::getfilepath(metadata_ts_to_cache,dbdir());
                            if(0 == sv_stat(getLongPathName(metadatapathname.c_str()).c_str(),&statbuf))
                            {
                                m_readaheadptr -> read_ahead(metadata_ts_to_cache, dbdir());
                                --readahead_file_count;
                            }
                            metadata_ts_to_cache -=HUNDREDS_OF_NANOSEC_IN_HOUR;
                            m_lastcache_ts = metadata_ts_to_cache;
                        }

                        m_metadatafileptr = m_readaheadptr ->get_metadata_ptr(m_metadatafiletime);
                        if(!m_metadatafileptr)
                        {
                            m_readnextmetadatafile = true;
                            m_metadatafiletime -= HUNDREDS_OF_NANOSEC_IN_HOUR;
                            readahead = false;
                            continue;
                        }

                    } else
                    {
                        std::string metadatapathname = cdpv3metadatafile_t::getfilepath(m_metadatafiletime,dbdir());
                        m_metadatafileptr.reset(new cdpv3metadatafile_t(metadatapathname,-1,m_readahead_length,true));
                    }

                    m_metadatafiletime -= HUNDREDS_OF_NANOSEC_IN_HOUR;
                }

                //if m_readnextmetadata is set to true then we will fetch metadata in desc
                // and check if last time stamp < recovery time then we will send the end of fetching drtd to caller
                // if drtd fetched < 1 then we will simply fetching next set of metadata
                if(m_readnextmetadata)
                {
                    m_cdpmetadata.clear();

                    SVERROR rv = m_metadatafileptr->read_metadata_desc(m_cdpmetadata, m_drtdcndn-> toTime());
                    m_readnextmetadata = false;
                    if(rv.failed())
                    {
                        good_status = false;
                        hr = SVE_FAIL;
                        break;
                    }

                    if(rv == SVS_FALSE)
                    {
                        m_readnextmetadatafile = true;
                        if(m_isreadaheadcache_enabled)
                        {
                            readahead = true;

							//Capturing and updating the IOs and length
							stats = m_metadatafileptr->get_io_stats();
							update_io_stats(stats);

                            m_readaheadptr -> close(m_metadatafiletime + HUNDREDS_OF_NANOSEC_IN_HOUR);
                        }
                        continue;
                    }


                    if((m_drtdcndn-> fromTime()) && (m_cdpmetadata.m_tsfc > m_drtdcndn-> fromTime()))
                    {
                        m_readnextmetadata = true;
                        continue;
                    }

                    if(m_cdpmetadata.m_type & DIFFTYPE_DIFFSYNC_BIT_SET)
                    {
                        if(!m_drtdcndn-> toSequenceId()	&& (m_cdpmetadata.m_tslc  < m_drtdcndn-> toTime()))
                        {
                            hr = SVS_FALSE; 
                            break;
                        }


                        if((m_cdpmetadata.m_type & DIFFTYPE_PERIO_BIT_SET) &&
                            (m_cdpmetadata.m_type & DIFFTYPE_NONSPLITIO_BIT_SET) &&
                            m_drtdcndn-> toSequenceId() &&
                            ( m_cdpmetadata.m_tslcseq <= m_drtdcndn-> toSequenceId()))
                        {
                            hr = SVS_FALSE;
                            break;
                        }
                    }

                    m_drtd_idx = m_cdpmetadata.m_drtds.size() -1;
                }

                //if all the diffinfos are processed then we are setting m_drtd_idx to zero for fetching
                // next set of metadata
                if(m_drtd_idx < 0)
                {
                    bool bkmk_found = false;
                    if(!m_drtdcndn-> toEvent().empty() && (m_drtdcndn ->toTime() == m_cdpmetadata.m_tsfc))
                    {
                        for(int i = 0; i < m_cdpmetadata.m_users.size() ; ++i)
                        {
                            SvMarkerPtr bkmkptr = m_cdpmetadata.m_users[i];
                            if(!strcmp(bkmkptr -> Tag().get(),m_drtdcndn ->toEvent().c_str()))
                            {
                                bkmk_found = true;
                                break;
                            }
                        }
                    }

                    if(bkmk_found)
                    {
                        hr = SVS_FALSE;
                        break;
                    }else
                    {
                        m_readnextmetadata = true;
                        continue;
                    }
                }

                cdp_drtdv2_t drtd = m_cdpmetadata.m_drtds[m_drtd_idx];
                // the current drtd contains the change greater than upper bound, 
                // so increamenting the iterator and continuing
                if((m_drtdcndn-> fromTime()) && 
                    ((m_cdpmetadata.m_tsfc + drtd.get_timedelta()) > m_drtdcndn-> fromTime()))
                {
                    --m_drtd_idx;
                    continue;
                }

                //checking if the drtd time is less than equal to recovery time 
                //then stop fetching drtd and reports end of fetching
                if( m_drtdcndn-> toEvent().empty() 
                    && (m_cdpmetadata.m_type & DIFFTYPE_DIFFSYNC_BIT_SET)
                    && (m_cdpmetadata.m_type & DIFFTYPE_NONSPLITIO_BIT_SET)
                    && !m_drtdcndn-> toSequenceId()
                    && ((m_cdpmetadata.m_tsfc + drtd.get_timedelta()) <= m_drtdcndn-> toTime() ))
                {
                    hr = SVS_FALSE; 
                    break;
                }

                //checking if the drtd time is less than equal to recovery time 
                //then stop fetching drtd and reports end of fetching
                if( m_drtdcndn-> toSequenceId()	&& 
                    (m_cdpmetadata.m_type & DIFFTYPE_DIFFSYNC_BIT_SET) &&
                    (m_cdpmetadata.m_type & DIFFTYPE_NONSPLITIO_BIT_SET) &&
                    (m_cdpmetadata.m_type & DIFFTYPE_PERIO_BIT_SET)	&&
                    ((m_cdpmetadata.m_tsfcseq + drtd.get_seqdelta()) <= m_drtdcndn-> toSequenceId())
                    )
                {
                    hr = SVS_FALSE;//finished reading drtds
                    break;
                }

                if(hr == SVS_FALSE)
                    break;

                //filling up the drtds
                rollback_drtd.voloffset = drtd.get_volume_offset();
                rollback_drtd.length = drtd.get_length();
                rollback_drtd.fileoffset = drtd.get_file_offset();
                rollback_drtd.filename = m_cdpmetadata.m_filenames[drtd.get_fileid()];
                rollback_drtd.timestamp = m_cdpmetadata.m_tsfc + drtd.get_timedelta();

                hr = SVS_OK;
                --m_drtd_idx;
                break;
            }

        } while (0);
    }

    catch(exception &ex){
        std::stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured while reading DRTD\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        good_status = false;
        hr = SVE_FAIL;

    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    if(hr == SVE_FAIL || hr == SVS_FALSE)
    {
        m_readaheadptr.reset();
    }
    return hr;
}

bool CDPV3DatabaseImpl::getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    try
    {
        connect();

        getrecoveryrange_nolock(start,end);

        shutdown();
        rv = true;

    } catch( std::exception & ex )	
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while retrieving the summary from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV3DatabaseImpl::get_retention_info(CDPInformation & info)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do
        {
            getcdpsummary_nolock(info.m_summary);
            getcdptimeranges_nolock(info.m_ranges);
            getcdpevents_nolock(info.m_events);

        } while (0);


        shutdown();


    } catch( std::exception & ex )	{
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while retrieving retention information from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}




void CDPV3DatabaseImpl::getcdptimeranges_nolock(CDPTimeRanges & ranges)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());


    std::stringstream query;
    query << "select * from " << CDPV3RECOVERYRANGESTBLNAME ;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    while(m_reader.read())
    {
        CDPTimeRange timerange;

        timerange.c_entryno = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_ENTRYNO_COL);
        timerange.c_starttime = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFC_COL);
        timerange.c_endtime = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLC_COL);
        timerange.c_startseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFCSEQ_COL);
        timerange.c_endseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLCSEQ_COL);
        timerange.c_granularity = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_GRANULARITY_COL));
        timerange.c_mode = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_ACCURACY_COL));
        SV_UINT status = m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_CXUPDATESTATUS_COL);
        if(status == CS_DELETE_PENDING)
        {
            timerange.c_action = CDP_TIMERANGE_DELETE_REQUEST;
        } else
        {
            timerange.c_action = CDP_TIMERANGE_INSERT_REQUEST;
        }


        ranges.push_back(timerange);
    } 

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::getcdpevents_nolock(CDPEvents & cdpevents)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s \n",FUNCTION_NAME, m_dbname.c_str());

    std::stringstream query;
    query << "select * from " << CDPV3BOOKMARKSTBLNAME ;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    while(m_reader.read())
    {
        CDPEvent cdpevent;

        cdpevent.c_eventno =  m_reader.getint64(CDPV3_BOOKMARKSTBL_C_ENTRYNO_COL);;
        cdpevent.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_APPTYPE_COL));
        cdpevent.c_eventvalue = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_VALUE_COL); 
        cdpevent.c_eventmode = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_ACCURACY_COL));
        cdpevent.c_eventtime = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_TIMESTAMP_COL);
        cdpevent.c_eventseq = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_SEQUENCE_COL);
        cdpevent.c_identifier = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_IDENTIFIER_COL);
        cdpevent.c_verified = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_VERIFIED_COL));
        cdpevent.c_comment = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_COMMENT_COL);
        SV_UINT  status = m_reader.getint(CDPV3_BOOKMARKSTBL_C_CXUPDATESTATUS_COL);
        if(status == CS_DELETE_PENDING)
        {
            cdpevent.c_status = CDP_BKMK_DELETE_REQUEST;
        }else
        {
            if(cdpevent.c_verified == VERIFIEDEVENT)
            {
                cdpevent.c_status = CDP_BKMK_UPDATE_REQUEST;
            } else 
            {
                cdpevent.c_status = CDP_BKMK_INSERT_REQUEST;
            }
        }
        //this is added as part of upgrade call
        //as new field c_lockstatus is added into database so once upgrade is done and service is in stop state 
        //then all cdpcli command should work

        double version;
        double revision;
        get_version_nolock(version, revision);
        if(revision >= CDPV3VER3REV1)
        {
            cdpevent.c_lockstatus = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_LOCKED_COL));
        }
        else
        {
            cdpevent.c_lockstatus = BOOKMARK_STATE_UNLOCKED; 
        }

        if(revision >= CDPV3VER3REV2)
        {
            cdpevent.c_lifetime = static_cast<SV_ULONGLONG> (m_reader.getint64(CDPV3_BOOKMARKSTBL_C_LIFETIME_COL ));
        }
        else
        {
            cdpevent.c_lifetime = INM_BOOKMARK_LIFETIME_NOTSPECIFIED; 
        }

        cdpevents.push_back(cdpevent);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV3DatabaseImpl::get_pending_updates(CDPInformation & info,const SV_ULONG & record_limit)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do
        {
            getcdpsummary_nolock(info.m_summary);
            getcdppendingtimeranges_nolock(info.m_ranges,record_limit);
            getcdppendingevents_nolock(info.m_events,record_limit);

        } while (0);


        shutdown();


    } catch( std::exception & ex )	{
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while retrieving updates for CS from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


void CDPV3DatabaseImpl::getcdppendingtimeranges_nolock(CDPTimeRanges & ranges,const SV_ULONG & records_to_fetch)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    SV_UINT status = 0;
    std::stringstream query;
    query << "select * from " << CDPV3RECOVERYRANGESTBLNAME 
        << " where c_cxupdatestatus != " 
        << CS_ADD_DONE ; 
    if(records_to_fetch)
        query << " limit " << records_to_fetch; 

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    while(m_reader.read())
    {
        CDPTimeRange timerange;

        timerange.c_entryno = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_ENTRYNO_COL);
        timerange.c_starttime = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFC_COL);
        timerange.c_endtime = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLC_COL);
        timerange.c_startseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFCSEQ_COL);
        timerange.c_endseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLCSEQ_COL);
        timerange.c_granularity = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_GRANULARITY_COL));
        timerange.c_mode = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_ACCURACY_COL));
        status = m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_CXUPDATESTATUS_COL);
        if(status == CS_DELETE_PENDING)
        {
            timerange.c_action = CDP_TIMERANGE_DELETE_REQUEST;
        } else
        {
            timerange.c_action = CDP_TIMERANGE_INSERT_REQUEST;
        }

        ranges.push_back(timerange);
    } 

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::getcdppendingevents_nolock(CDPEvents & cdpevents,const SV_ULONG & records_to_fetch)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s \n",FUNCTION_NAME, m_dbname.c_str());

    SV_UINT status = 0;
    SV_ULONGLONG count = 0;
    std::stringstream query;
    query << "select * from " << CDPV3BOOKMARKSTBLNAME 
        << " where c_cxupdatestatus != " 
        << CS_ADD_DONE ;
    if(records_to_fetch)
        query << " limit " << records_to_fetch;


    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    while(m_reader.read())
    {
        CDPEvent cdpevent;

        cdpevent.c_eventno =  m_reader.getint64(CDPV3_BOOKMARKSTBL_C_ENTRYNO_COL);;
        cdpevent.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_APPTYPE_COL));
        cdpevent.c_eventvalue = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_VALUE_COL); 
        cdpevent.c_eventmode = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_ACCURACY_COL));
        cdpevent.c_eventtime = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_TIMESTAMP_COL);
        cdpevent.c_eventseq = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_SEQUENCE_COL);
        cdpevent.c_identifier = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_IDENTIFIER_COL);
        cdpevent.c_verified = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_VERIFIED_COL));
        cdpevent.c_comment = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_COMMENT_COL);
        status = m_reader.getint(CDPV3_BOOKMARKSTBL_C_CXUPDATESTATUS_COL);
        if(status == CS_DELETE_PENDING)
        {
            cdpevent.c_status = CDP_BKMK_DELETE_REQUEST;
        } else
        {
            if(cdpevent.c_verified == VERIFIEDEVENT)
            {
                cdpevent.c_status = CDP_BKMK_UPDATE_REQUEST;
            } else 
            {
                cdpevent.c_status = CDP_BKMK_INSERT_REQUEST;
            }
        }
        //this is added as part of upgrade call
        //as new field c_lockstatus is added into database so once upgrade is done and service is in stop state 
        //then all cdpcli command should work

        double version;
        double revision;
        get_version_nolock(version, revision);
        if(revision >= CDPV3VER3REV1)
        {
            cdpevent.c_lockstatus = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_LOCKED_COL));
        }
        else
        {
            cdpevent.c_lockstatus = BOOKMARK_STATE_UNLOCKED; 
        }

        if(revision >= CDPV3VER3REV2)
        {
            cdpevent.c_lifetime = static_cast<SV_ULONGLONG> (m_reader.getint64(CDPV3_BOOKMARKSTBL_C_LIFETIME_COL ));
        }
        else
        {
            cdpevent.c_lifetime = INM_BOOKMARK_LIFETIME_NOTSPECIFIED; 
        }

        cdpevents.push_back(cdpevent);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV3DatabaseImpl::delete_pending_updates(CDPInformation & info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do
        {
            BeginTransaction();
            deletecdppendingtimeranges_nolock(info.m_ranges);
            deletecdppendingevents_nolock(info.m_events);
            CommitTransaction();

        } while (0);


        shutdown();


    } catch( std::exception & ex )	{
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while updating CS information to the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


void CDPV3DatabaseImpl::deletecdppendingtimeranges_nolock(CDPTimeRanges & ranges)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    CDPTimeRanges::iterator timeranges_iter = ranges.begin();
    CDPTimeRanges::iterator timeranges_end = ranges.end();

    for( ; timeranges_iter != timeranges_end ; ++timeranges_iter)
    {
        CDPTimeRange timerange = *timeranges_iter;

        std::stringstream updatequery;
        if(timerange.c_action == CDP_TIMERANGE_INSERT_REQUEST)
        {
            updatequery << "update " << CDPV3RECOVERYRANGESTBLNAME 
                << " set c_cxupdatestatus = " 
                << CS_ADD_DONE
                << " where c_entryno = " 
                << timerange.c_entryno
                << " and c_tslc = "
                << timerange.c_endtime;
        } else
        {
            updatequery << " delete  from " << CDPV3RECOVERYRANGESTBLNAME
                << " where c_entryno = "
                << timerange.c_entryno ;
        }

        sqlite3_command update_cmd(*m_con, updatequery.str() );
        update_cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}


void CDPV3DatabaseImpl::deletecdppendingevents_nolock(CDPEvents & events)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    CDPEvents::iterator events_iter = events.begin();
    CDPEvents::iterator events_end = events.end();

    for( ; events_iter != events_end ; ++events_iter)
    {
        CDPEvent curr_event = *events_iter;

        std::stringstream delquery;
        if(curr_event.c_status == CDP_BKMK_DELETE_REQUEST)
        {
            delquery << " delete from " << CDPV3BOOKMARKSTBLNAME
                << " where c_entryno = "
                << curr_event.c_eventno;

        } else
        {
            delquery << " update " << CDPV3BOOKMARKSTBLNAME
                << " set c_cxupdatestatus = " 
                << CS_ADD_DONE
                << " where c_entryno = " 
                << curr_event.c_eventno;
        }

        sqlite3_command del_cmd(*m_con, delquery.str() );
        del_cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV3DatabaseImpl::delete_all_pending_updates()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do
        {
            BeginTransaction();
            deleteallpendingtimeranges_nolock();
            deleteallpendingevents_nolock();
            CommitTransaction();

        } while (0);


        shutdown();


    } catch( std::exception & ex )	{
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while updating CS information to the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

void CDPV3DatabaseImpl::deleteallpendingtimeranges_nolock()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

	std::stringstream updatequery1, updatequery2;
	std::vector<std::string> stmts;

	updatequery1 << "update " << CDPV3RECOVERYRANGESTBLNAME 
		<< " set c_cxupdatestatus = " 
		<< CS_ADD_DONE
		<< " where c_cxupdatestatus = "
		<< CS_ADD_PENDING;

	updatequery2 << " delete  from " << CDPV3RECOVERYRANGESTBLNAME
		<< " where c_cxupdatestatus = "
		<< CS_DELETE_PENDING ;

	stmts.push_back(updatequery1.str());
	stmts.push_back(updatequery2.str());

	executestmts(stmts);

	DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::deleteallpendingevents_nolock()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

	std::stringstream updatequery1, updatequery2;
	std::vector<std::string> stmts;

	updatequery1 << " update " << CDPV3BOOKMARKSTBLNAME
		<< " set c_cxupdatestatus = " 
		<< CS_ADD_DONE
		<< " where c_cxupdatestatus = " 
		<< CS_ADD_PENDING;

	updatequery2 << " delete from " << CDPV3BOOKMARKSTBLNAME
		<< " where c_cxupdatestatus = "
		<< CS_DELETE_PENDING;

	stmts.push_back(updatequery1.str());
	stmts.push_back(updatequery2.str());

	executestmts(stmts);

	DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV3DatabaseImpl::update_verified_events(const CDPMarkersMatchingCndn & cndn)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;
    int rec_count = 0;


    try
    {
        connect();

        do
        {
            BeginTransaction();
            update_event_query_nolock(cndn);
            rec_count = m_con->rowsmodified();
            CommitTransaction();
			if (rec_count == 0)
			{
				DebugPrintf(SV_LOG_INFO, "There are no matching bookmarks for the specified input.\n");
                rv = false;
                break;
            }

        } while (0);


        shutdown();


    } catch( std::exception & ex )	{
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while updating CS information to the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

/*
* FUNCTION NAME : update_locked_bookmarks
*
* DESCRIPTION : mark bookmark as locked
*
* INPUT PARAMETERS : cndn
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
bool CDPV3DatabaseImpl::update_locked_bookmarks( const CDPMarkersMatchingCndn & cndn,std::vector<CDPEvent> &cdpEvents,SV_USHORT &newstatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;
    //fetch the event information for any updation 

    try
    {
        fetch_locked_bookmarks(cndn,cdpEvents);
        connect();
        do
        {
            if(cdpEvents.empty() )
            {
                m_reader.close();
                rv = false;
                break;
            }
            BeginTransaction();
            update_locked_bookmarks_query(cndn,newstatus);
            CommitTransaction();            
        } while (0);


        shutdown();


    } catch( std::exception & ex )	{
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while marking locked bookmarks to the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}
/*
* FUNCTION NAME : fetch_locked_bookmarks
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : cndn
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
void CDPV3DatabaseImpl::fetch_locked_bookmarks(const CDPMarkersMatchingCndn & cndn,std::vector<CDPEvent> &cdpEvents)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    connect();
    do
    {
        std::string query;
        form_event_query(cndn, query);
        sqlite3_command * cmd = new sqlite3_command(*(m_con),query);
        m_cmd.reset(cmd);
        m_reader = m_cmd -> executereader();

        CDPEvent cdpEvent;
        while(read(cdpEvent) == SVS_OK)
        {
            cdpEvents.push_back(cdpEvent);
        }
        m_reader.close();

    } while (0);

    shutdown();

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::update_event_query_nolock(const CDPMarkersMatchingCndn & cndn)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    std::vector<std::string> clause;

    if(cndn.type())
    {
        std::stringstream tmp;
        tmp << "c_apptype =" << cndn.type();
        clause.push_back(tmp.str());
    }

    if(!cndn.value().empty())
    {
        std::stringstream tmp;
        tmp << "c_value =" << "\"" << cndn.value() << "\"" ;
        clause.push_back(tmp.str());
    }

    if(!cndn.identifier().empty())
    {
        std::stringstream tmp;
        tmp << "c_identifier =" << "\"" << cndn.identifier() << "\"";
        clause.push_back(tmp.str());
    }

    if(cndn.atTime())
    {
        std::stringstream tmp;
        tmp << "c_timestamp =" << cndn.atTime();
        clause.push_back(tmp.str());
    }

    std::stringstream updatequery;
    updatequery << "update " << CDPV3BOOKMARKSTBLNAME 
        << " set " 
        << " c_verified = " << VERIFIEDEVENT <<","
        << " c_comment = " << "\"" << cndn.comment() << "\"" << ","
        << " c_cxupdatestatus = " << CS_ADD_PENDING ;

    std::vector<std::string>::iterator iter     = clause.begin();
    std::vector<std::string>::iterator iter_end = clause.end();
    if(iter != iter_end)
    {
        updatequery << " where " << *iter ;
        ++iter;
    }

    for ( ; iter != iter_end ; ++iter )
    {
        updatequery << " and " << *iter ;
    }


    sqlite3_command update_cmd(*m_con, updatequery.str() );
    update_cmd.executenonquery();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}

/*
* FUNCTION NAME : update_locked_bookmarks_query
*
* DESCRIPTION : mark the bookmark as locked/unlocked for given criteria
*
* INPUT PARAMETERS : cndn
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
void CDPV3DatabaseImpl::update_locked_bookmarks_query(const CDPMarkersMatchingCndn & cndn,SV_USHORT &newstatus)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    std::vector<std::string> clause;

    if(cndn.type())
    {
        std::stringstream tmp;
        tmp << "c_apptype =" << cndn.type();
        clause.push_back(tmp.str());
    }

    if(!cndn.value().empty())
    {
        std::stringstream tmp;
        tmp << "c_value =" << "\"" << cndn.value() << "\"" ;
        clause.push_back(tmp.str());
    }

    if(!cndn.identifier().empty())
    {
        std::stringstream tmp;
        tmp << "c_identifier =" << "\"" << cndn.identifier() << "\"";
        clause.push_back(tmp.str());
    }
    if(cndn.atTime())
    {
        std::stringstream tmp;
        tmp << "c_timestamp =" << cndn.atTime();
        clause.push_back(tmp.str());
    }

    if(cndn.lockstatus() != BOOKMARK_STATE_BOTHLOCKED_UNLOCKED 
        && cndn.lockstatus() != BOOKMARK_STATE_LOCKED)
    {
        std::stringstream tmp;               
        tmp << "c_lockstatus =" << cndn.lockstatus();
        clause.push_back(tmp.str());
    }

    if(cndn.lockstatus() == BOOKMARK_STATE_LOCKED)
    {
        std::stringstream tmp;               
        tmp << "c_lockstatus !=" << BOOKMARK_STATE_UNLOCKED;
        clause.push_back(tmp.str());
    }

    // Append the given comment for bookmark locking
    std::string tmp_comment;
    if(!cndn.comment().empty() )
    {
        tmp_comment = "\n" ;
        tmp_comment+=cndn.comment() ;
    }

    std::stringstream updatequery;
    updatequery << "update " << CDPV3BOOKMARKSTBLNAME 
        << " set " 
        << " c_comment = " << "c_comment ||" << "\"" << tmp_comment << "\"" << ","
        << " c_lockstatus = " << newstatus;

    std::vector<std::string>::iterator iter     = clause.begin();
    std::vector<std::string>::iterator iter_end = clause.end();
    if(iter != iter_end)
    {
        updatequery << " where " << *iter ;
        ++iter;
    }

    for ( ; iter != iter_end ; ++iter )
    {
        updatequery << " and " << *iter ;
    }

    DebugPrintf(SV_LOG_DEBUG,"update query: %s\n",updatequery.str().c_str());
    sqlite3_command update_cmd(*m_con, updatequery.str() );
    update_cmd.executenonquery();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
}


bool  CDPV3DatabaseImpl::update_db(const CDPV3RecoveryRange & timerangeupdate,
                                   std::vector<CDPV3BookMark> & bookmarks,
                                   bool revoke)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;
    CDPV3RecoveryRange latestrange;
    SV_ULONGLONG latestrange_endts_inhrs;
    SV_ULONGLONG newrange_startts_inhrs;

    try
    {
        connect();


        do
        {
            BeginTransaction();
            fetch_latest_timerange(latestrange);

            if(!CDPUtil::ToTimestampinHrs(timerangeupdate.c_tsfc, newrange_startts_inhrs))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s invalid timestamp " ULLSPEC " \n",
                    FUNCTION_NAME, timerangeupdate.c_tsfc);
                rv = false;
                break;
            }

            if(latestrange.c_entryno)
            {

                if(!CDPUtil::ToTimestampinHrs(latestrange.c_tslc, latestrange_endts_inhrs))
                {
                    DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s invalid timestamp " ULLSPEC " in db\n",
                        FUNCTION_NAME, latestrange.c_tsfc);
                    rv = false;
                    break;
                }

                if(latestrange_endts_inhrs == newrange_startts_inhrs)
                {
                    if(latestrange.c_accuracy == timerangeupdate.c_accuracy)
                    {
                        update_timerange_nolock(latestrange, timerangeupdate);
                    } else
                    {
                        insert_timerange_nolock(timerangeupdate);
                    }
                } else
                {
                    CDPV3RecoveryRange oldhrupdate;
                    oldhrupdate.c_tsfc = latestrange.c_tsfc;
                    oldhrupdate.c_tsfcseq = latestrange.c_tsfcseq;
                    oldhrupdate.c_tslc = latestrange_endts_inhrs + HUNDREDS_OF_NANOSEC_IN_HOUR -1;
                    oldhrupdate.c_tslcseq = latestrange.c_tslcseq;
                    oldhrupdate.c_granularity = latestrange.c_granularity;
                    oldhrupdate.c_accuracy = latestrange.c_accuracy;
                    oldhrupdate.c_cxupdatestatus = CS_ADD_PENDING;
                    update_timerange_nolock(latestrange, oldhrupdate);

                    CDPV3RecoveryRange newhrupdate;
                    newhrupdate.c_tsfc = newrange_startts_inhrs;
                    newhrupdate.c_tsfcseq = timerangeupdate.c_tsfcseq;
                    newhrupdate.c_tslc = timerangeupdate.c_tslc;
                    newhrupdate.c_tslcseq = timerangeupdate.c_tslcseq;
                    newhrupdate.c_granularity = timerangeupdate.c_granularity;
                    newhrupdate.c_accuracy = timerangeupdate.c_accuracy;
                    newhrupdate.c_cxupdatestatus = timerangeupdate.c_cxupdatestatus;
                    insert_timerange_nolock(newhrupdate);  
                }
            }else
            {
                insert_timerange_nolock(timerangeupdate);
            }

            if(!revoke)
            {
                insert_bookmarks_nolock(bookmarks);
            } else
            {
                revoke_bookmarks_nolock(bookmarks);
            }

            CommitTransaction();

        } while(0);

        shutdown();

    } catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Database:" << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


void CDPV3DatabaseImpl::fetch_latest_timerange(CDPV3RecoveryRange & timerange)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    std::stringstream query ;
    query << "select * from " <<  CDPV3RECOVERYRANGESTBLNAME
		<< " where c_cxupdatestatus != " << CS_DELETE_PENDING 
        << " order by c_tslc DESC, c_tslcseq DESC, c_entryno DESC limit 1";

    boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));
    sqlite3_reader reader = cmd -> executereader();
    if(!reader.read())
    {
        timerange.c_entryno = 0;
        timerange.c_tsfc = 0;
        timerange.c_tslc = 0;
        timerange.c_tsfcseq = 0;
        timerange.c_tslcseq = 0;
        timerange.c_granularity = 0;
        timerange.c_accuracy = 0;
        timerange.c_cxupdatestatus = 0;
    }else
    {
        timerange.c_entryno = reader.getint64(CDPV3_RECOVERYRANGESTBL_C_ENTRYNO_COL);
        timerange.c_tsfc = reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFC_COL);
        timerange.c_tslc = reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLC_COL);
        timerange.c_tsfcseq = reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFCSEQ_COL);
        timerange.c_tslcseq = reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLCSEQ_COL);
        timerange.c_granularity = reader.getint(CDPV3_RECOVERYRANGESTBL_C_GRANULARITY_COL);
        timerange.c_accuracy = reader.getint(CDPV3_RECOVERYRANGESTBL_C_ACCURACY_COL);
        timerange.c_cxupdatestatus = reader.getint(CDPV3_RECOVERYRANGESTBL_C_CXUPDATESTATUS_COL);
    }

    reader.close();
    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::update_timerange_nolock(const CDPV3RecoveryRange & latestrange, 
                                                const CDPV3RecoveryRange &timerangeupdate)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    std::stringstream updatequery;
    updatequery << "update " << CDPV3RECOVERYRANGESTBLNAME 
        << " set " 
        << " c_tslc = " << timerangeupdate.c_tslc << ","
        << " c_tslcseq =  " << timerangeupdate.c_tslcseq << ","
        << " c_cxupdatestatus = " << CS_ADD_PENDING 
        << " where c_entryno = " << latestrange.c_entryno ;

    DebugPrintf(SV_LOG_DEBUG, "%s\n", updatequery.str().c_str());

    sqlite3_command update_cmd(*m_con, updatequery.str() );
    update_cmd.executenonquery();
    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::insert_timerange_nolock(const CDPV3RecoveryRange &timerange)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    std::stringstream insertquery;
    insertquery << "insert into " << CDPV3RECOVERYRANGESTBLNAME 
        << " ( c_tsfc, c_tslc, c_tsfcseq, c_tslcseq, c_granularity, c_accuracy, c_cxupdatestatus )"
        << " values(" 
        <<  timerange.c_tsfc << "," 
        <<  timerange.c_tslc << "," 
        <<  timerange.c_tsfcseq << ","
        <<  timerange.c_tslcseq << ","
        <<  timerange.c_granularity << ","
        <<  timerange.c_accuracy << ","
        <<  timerange.c_cxupdatestatus
        << ");" ;

    sqlite3_command insert_cmd(*m_con, insertquery.str() );
    insert_cmd.executenonquery();
    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::insert_bookmarks_nolock(std::vector<CDPV3BookMark> & bookmarks)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    std::vector<CDPV3BookMark>::iterator bkmk_iter = bookmarks.begin();
    std::vector<CDPV3BookMark>::iterator bkmk_end = bookmarks.end();
    for( ; bkmk_iter != bkmk_end ; ++bkmk_iter)
    {

        std::stringstream insertquery;
        insertquery << "insert into " << CDPV3BOOKMARKSTBLNAME 
            << " ( c_apptype, c_value, c_accuracy, c_timestamp, c_sequence, c_identifier, c_verified,c_comment,c_cxupdatestatus,c_counter,c_lockstatus, c_lifetime )"
            << " values(" 
            <<  bkmk_iter -> c_apptype << ","
            <<  '?' << "," 
            <<  bkmk_iter -> c_accuracy << "," 
            <<  bkmk_iter -> c_timestamp << ","
            <<  bkmk_iter -> c_sequence << ","
            <<  '?' << ","
            <<  bkmk_iter -> c_verified << ","
            <<  '?' << ","
            <<  CS_ADD_PENDING << ","
            <<  bkmk_iter ->c_counter << ","
            <<  bkmk_iter ->c_lockstatus << ","
            <<  bkmk_iter ->c_lifetime
            << ");" ;

        DebugPrintf(SV_LOG_DEBUG, "%s\n", insertquery.str().c_str());

        sqlite3_command cmd(*m_con, insertquery.str() );
        cmd.bind( 1, bkmk_iter -> c_value );
        cmd.bind(2, bkmk_iter -> c_identifier);
        cmd.bind(3, bkmk_iter -> c_comment);
        cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV3DatabaseImpl::revoke_bookmarks_nolock(std::vector<CDPV3BookMark> & bookmarks)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    std::vector<CDPV3BookMark>::iterator bkmk_iter = bookmarks.begin();
    std::vector<CDPV3BookMark>::iterator bkmk_end = bookmarks.end();
    for( ; bkmk_iter != bkmk_end ; ++bkmk_iter)
    {

        std::stringstream fetch_revokation_timestamp_query;
        fetch_revokation_timestamp_query << "select c_timestamp,c_sequence,c_identifier from " 
            << CDPV3BOOKMARKSTBLNAME 
            << " where c_apptype = " << bkmk_iter -> c_apptype
            << " and c_value = " << '?' ;

        sqlite3_command cmd(*m_con, fetch_revokation_timestamp_query.str() );
        cmd.bind( 1, bkmk_iter -> c_value );


        DebugPrintf(SV_LOG_DEBUG, "%s\n", fetch_revokation_timestamp_query.str().c_str());
        sqlite3_reader reader= cmd.executereader();
        if(reader.read())
        {
            SV_ULONGLONG revokation_ts = reader.getint64(0);
            SV_ULONGLONG revokation_seq = reader.getint64(1);
            std::string revokation_id = reader.getblob(2);

            std::stringstream revokequery;
            revokequery << " update " << CDPV3BOOKMARKSTBLNAME
                << " set c_cxupdatestatus = " 
                << CS_DELETE_PENDING
                << " where c_timestamp = " << revokation_ts 
                << " and c_sequence = " << revokation_seq 
                << " and c_identifier = " << '?' ;



            sqlite3_command revoke_cmd(*m_con, revokequery.str() );
            revoke_cmd.bind(1,revokation_id);
            DebugPrintf(SV_LOG_DEBUG, "%s\n", revokequery.str().c_str());

            revoke_cmd.executenonquery();

            std::string human_readable_ts;
            CDPUtil::ToDisplayTimeOnConsole(revokation_ts, human_readable_ts);
            DebugPrintf(SV_LOG_DEBUG, "revoked tags issued at time %s\n",
                human_readable_ts.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
}


// =============================================================
// coalesce routines
// =============================================================

bool CDPV3DatabaseImpl::enforce_sparse_policies(const CDP_SETTINGS & settings,
                                                const SV_ULONGLONG & tsfc_hr, const SV_ULONGLONG & tslc_hr,const std::string& volname)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        do
        {
            bool done = false;
            SV_UINT duration = 0;
            SV_ULONGLONG coalesce_done_period = 0;
            SV_ULONGLONG coalesce_start_ts = 0;
            SV_ULONGLONG coalesce_end_ts = 0;

            cdp_policies_t::const_iterator piter = settings.cdp_policies.begin();
            cdp_policies_t::const_iterator pend = settings.cdp_policies.end();

            for( ; piter != pend; ++piter)
            {
                cdp_policy policy = *piter;

                // maintain all writes
                if(0 == policy.granularity)
                {
                    continue;
                }

                for( duration = 0 ; duration < policy.duration ; duration += policy.granularity)
                {
                    coalesce_done_period = (policy.start + duration) * HUNDREDS_OF_NANOSEC_IN_HOUR;

                    coalesce_end_ts = tslc_hr - coalesce_done_period;
                    coalesce_start_ts = coalesce_end_ts - (policy.granularity * HUNDREDS_OF_NANOSEC_IN_HOUR);
                    if(coalesce_end_ts < tsfc_hr)
                    {
                        done = true;
                        break;
                    }

                    // fetch the bookmarks in the coalesce_time_range
                    // find bookmarks to retain and bookmarks to delete 
                    // get the timestamps to preserve in the coalesce time range
                    // fetch the timestamps present in the db
                    // if there are no bookmarks to delete and timestamps present match
                    // with the required, coalescing is already done for this period
                    // move to next period
                    // else call delete_coalesced_datafiles
                    // db operations:
                    // mark the bookmarks to be deleted as CS_DELETE_PENDING
                    // mark the timerange entries in the region as CS_DELETE_PENDING
                    // insert the timestamps to be preserved

                    std::vector<CDPV3BookMark> bookmarks_in_db;
                    std::vector<CDPV3BookMark> bookmarks_to_preserve;
                    std::vector<CDPV3BookMark> bookmarks_to_delete;
                    std::vector<CDPV3RecoveryRange> timestamps_to_preserve;
                    std::vector<CDPV3RecoveryRange> timeranges_in_db;
                    std::vector<cdp_timestamp_t> cdp_timestamps_to_keep;

                    if(!fetch_bookmarks_in_timerange(coalesce_start_ts,coalesce_end_ts, bookmarks_in_db))
                    {
                        rv = false;
                        break;
                    }

                    if(!filter_bookmarks(tslc_hr,policy.tagcount,policy.granularity,policy.apptype,policy.userbookmarks,
                        bookmarks_in_db, bookmarks_to_preserve, bookmarks_to_delete))
                    {
                        rv = false;
                        break;
                    }


                    if(!fetch_timestamps_to_preserve(bookmarks_to_preserve, timestamps_to_preserve))
                    {
                        rv = false;
                        break;
                    }

                    if(!fetch_timeranges_in_db(coalesce_start_ts, coalesce_end_ts, timeranges_in_db))
                    {
                        rv = false;
                        break;
                    }

                    if(!timeranges_in_db.empty())
                    {
                        CDPV3RecoveryRange starting_range = *(timeranges_in_db.begin());
                        CDPV3RecoveryRange starting_boundary;
                        starting_boundary.c_entryno = starting_range.c_entryno;
                        starting_boundary.c_tsfc = starting_range.c_tsfc;
                        starting_boundary.c_tslc = starting_range.c_tsfc;
                        starting_boundary.c_tsfcseq = starting_range.c_tsfcseq;
                        starting_boundary.c_tslcseq = starting_range.c_tsfcseq;
                        starting_boundary.c_granularity = CDP_ONLY_END_POINTS;
                        starting_boundary.c_accuracy = starting_range.c_accuracy;
                        starting_boundary.c_cxupdatestatus = CS_ADD_PENDING;

                        timestamps_to_preserve.push_back(starting_boundary);

                        cdp_timestamp_t cdp_ts;
                        SV_TIME svtime;
                        if(!CDPUtil::ToSVTime(starting_range.c_tsfc,svtime))
                        {
                            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                                FUNCTION_NAME, starting_range.c_tsfc);
                            rv = false;
                            break;
                        }

                        cdp_ts.year = svtime.wYear - CDP_BASE_YEAR;
                        cdp_ts.month = svtime.wMonth;
                        cdp_ts.days = svtime.wDay;
                        cdp_ts.hours = svtime.wHour;
                        cdp_ts.events = 0;
                        cdp_timestamps_to_keep.push_back(cdp_ts);
                    }

                    if(is_already_coalesced(bookmarks_to_delete, timeranges_in_db, timestamps_to_preserve))
                    {
                        continue;
                    }


                    if(!fetch_cdptimestamps(bookmarks_to_preserve, cdp_timestamps_to_keep))
                    {
                        rv = false;
                        break;
                    }

					if(IsVsnapDriverAvailable())
                    {
                        CDPUtil::UnmountVsnapsInTimeRange(coalesce_start_ts,coalesce_end_ts,volname,bookmarks_to_preserve);
                    }

                    BeginTransaction();

                    if(!delete_coalesced_bookmarks(bookmarks_to_delete))
                    {
                        rv = false;
                        break;
                    }

                    if(!delete_coalesced_timeranges(timeranges_in_db))
                    {
                        rv = false;
                        break;
                    }

                    if(!add_coalesced_timestamps(timestamps_to_preserve))
                    {
                        rv = false;
                        break;
                    }

                    CommitTransaction();

                    if(!delete_coalesced_datafiles(coalesce_start_ts, coalesce_end_ts, bookmarks_to_preserve,
                        cdp_timestamps_to_keep))
                    {
                        rv = false;
                        break;
                    }
                }

                if(!rv)
                    break;

                if(done)
                    break;
            }

        } while (0);

    }	catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Database:" << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";
        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::fetch_bookmarks_in_timerange(const SV_ULONGLONG & start_ts,
                                                     const SV_ULONGLONG & end_ts,
                                                     std::vector<CDPV3BookMark> & bookmarks)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    std::stringstream query;
    query << "select * from " << CDPV3BOOKMARKSTBLNAME 
        << " where c_timestamp >= "  << start_ts
        << " and c_timestamp < " << end_ts
        << " and c_cxupdatestatus != " << CS_DELETE_PENDING
        << " order by c_timestamp ASC ";


    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    while(m_reader.read())
    {
        CDPV3BookMark cdpbookmark;

        cdpbookmark.c_entryno =  m_reader.getint64(CDPV3_BOOKMARKSTBL_C_ENTRYNO_COL);;
        cdpbookmark.c_apptype = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_APPTYPE_COL));
        cdpbookmark.c_value = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_VALUE_COL); 
        cdpbookmark.c_accuracy = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_ACCURACY_COL));
        cdpbookmark.c_timestamp = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_TIMESTAMP_COL);
        cdpbookmark.c_sequence = m_reader.getint64(CDPV3_BOOKMARKSTBL_C_SEQUENCE_COL);
        cdpbookmark.c_identifier = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_IDENTIFIER_COL);
        cdpbookmark.c_verified = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_VERIFIED_COL));
        cdpbookmark.c_comment = m_reader.getblob(CDPV3_BOOKMARKSTBL_C_COMMENT_COL);
        cdpbookmark.c_cxupdatestatus = m_reader.getint(CDPV3_BOOKMARKSTBL_C_CXUPDATESTATUS_COL);
        cdpbookmark.c_counter = m_reader.getint(CDPV3_BOOKMARKSTBL_C_COUNTER_COL);
        //this is added as part of upgrade call
        //as new field c_lockstatus is added into database so once upgrade is done and service is in stop state 
        //then all cdpcli command should work
        double version;
        double revision;
        get_version_nolock(version, revision);
        if(revision >= CDPV3VER3REV1)
        {
            cdpbookmark.c_lockstatus = static_cast<SV_USHORT> (m_reader.getint(CDPV3_BOOKMARKSTBL_C_LOCKED_COL));
        }
        else
        {
            cdpbookmark.c_lockstatus = BOOKMARK_STATE_UNLOCKED; 
        }

        if(revision >= CDPV3VER3REV2)
        {
            cdpbookmark.c_lifetime = static_cast<SV_ULONGLONG> (m_reader.getint64(CDPV3_BOOKMARKSTBL_C_LIFETIME_COL ));
        }
        else
        {
            cdpbookmark.c_lifetime = INM_BOOKMARK_LIFETIME_NOTSPECIFIED; 
        }

        bookmarks.push_back(cdpbookmark);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s\n", FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::filter_bookmarks(const SV_ULONGLONG & tslc_hr,
                                         const SV_UINT & tagcount,
                                         const SV_UINT & granularity,
                                         const SV_UINT & apptype,
                                         const std::vector<std::string> & userbookmarks,
                                         const std::vector<CDPV3BookMark> & bookmarks,
                                         std::vector<CDPV3BookMark> & bookmarks_to_preserve,
                                         std::vector<CDPV3BookMark> & bookmarks_to_delete)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    do
    {

        std::vector<CDPV3BookMark>::const_iterator biter = bookmarks.begin();
        std::vector<CDPV3BookMark>::const_iterator bend = bookmarks.end();
        std::vector<CDPV3BookMark> bookmarks_to_filter;
        std::vector<CDPV3BookMark> bookmarks_with_matching_timestamp;

        
        // 
        // you want 'tagcount' count of bookmarks in 
        // timerange 'granularity * HUNDREDS_OF_NANOSEC_IN_HOUR'
        // so distance (ie time) between any two tags should be
        // (granularity * HUNDREDS_OF_NANOSEC_IN_HOUR)/ tagcount
        //

        SV_ULONGLONG distance = (granularity * HUNDREDS_OF_NANOSEC_IN_HOUR)/ tagcount;

        // 
        SV_UINT bookmarks_to_drop = 0;

        std::vector<CDPV3BookMark> matching_bookmarks_in_retention;
        for( ; biter != bend; ++biter)
        {
            if((*biter).c_apptype == apptype)
            {
                if((*biter).c_lifetime == INM_BOOKMARK_LIFETIME_NOTSPECIFIED)
                {
                    if(apptype == STREAM_REC_TYPE_USERDEFINED_EVENT)
                    {
                        if(is_matching_userbookmark((*biter).c_value,userbookmarks))
                        {
                            matching_bookmarks_in_retention.push_back(*biter);
                        }
                        else
                        {
                            bookmarks_to_delete.push_back(*biter);
                        }
                    }
                    else
                    {
                        matching_bookmarks_in_retention.push_back(*biter);
                    }

                } else if((*biter).c_lifetime == INM_BOOKMARK_LIFETIME_FOREVER)
                {
                    bookmarks_to_preserve.push_back(*biter);
                } else // lifetime is specified in the bookmark
                {
                    if(is_expired(*biter, tslc_hr))
                    {
                        bookmarks_to_delete.push_back(*biter);
                    } else
                    {
                        bookmarks_to_preserve.push_back(*biter);
                    }
                }
            }
            else
            {
                bookmarks_to_filter.push_back(*biter);
            }
        } 

        if(tagcount > 0  && (bookmarks_to_preserve.size() < tagcount))
        {

            std::vector<CDPV3BookMark>::const_iterator bmiter = matching_bookmarks_in_retention.begin();
            std::vector<CDPV3BookMark>::const_iterator bmend = matching_bookmarks_in_retention.end();
            SV_UINT remaining_tagcount = tagcount - bookmarks_to_preserve.size();

            if(matching_bookmarks_in_retention.size() > remaining_tagcount)
            {
                bookmarks_to_drop = (matching_bookmarks_in_retention.size() - remaining_tagcount);

                SV_UINT bookmarks_dropped = 0;
                SV_ULONGLONG bookmark_ts  =0;

                for( ; bmiter != bmend ; ++bmiter)
                {
                    if((bookmarks_dropped < bookmarks_to_drop)
                        && ((*bmiter).c_timestamp - bookmark_ts < distance))
                    {
                        bookmarks_to_delete.push_back(*bmiter);
                        ++bookmarks_dropped;
                    } else
                    {
                        bookmarks_to_preserve.push_back(*bmiter);
                        bookmark_ts = (*bmiter).c_timestamp;
                    }			
                }

                assert(bookmarks_dropped == bookmarks_to_drop);
            }
            else
            {
                bookmarks_to_preserve.insert(bookmarks_to_preserve.end(), bmiter, bmend);
            }
        }

        if((0 == tagcount) && (STREAM_REC_TYPE_USERDEFINED_EVENT == apptype))
        {
            std::vector<CDPV3BookMark>::const_iterator bmiter = matching_bookmarks_in_retention.begin();
            std::vector<CDPV3BookMark>::const_iterator bmend = matching_bookmarks_in_retention.end();
            bookmarks_to_preserve.insert(bookmarks_to_preserve.end(), bmiter, bmend);
        }

        std::vector<CDPV3BookMark>::iterator bfiter = bookmarks_to_filter.begin();
        std::vector<CDPV3BookMark>::iterator bfend = bookmarks_to_filter.end();

        for( ; bfiter != bfend; ++bfiter)
        {
            if(is_matching_timestamp(bfiter ->c_timestamp,bfiter ->c_sequence,bookmarks_to_preserve))
            {
                bookmarks_with_matching_timestamp.push_back(*bfiter);
            }else
            {
                bookmarks_to_delete.push_back(*bfiter);
            }
        }

        bookmarks_to_preserve.insert(bookmarks_to_preserve.end(),
            bookmarks_with_matching_timestamp.begin(),
            bookmarks_with_matching_timestamp.end());
        // preserve bookmarks which are locked from getting dropped during coalesce operation
        {
            //moving bookmarks from deleted list to preserve which are locked 			
            remove_copy_if(bookmarks_to_delete.begin(),bookmarks_to_delete.end(),std::back_inserter(bookmarks_to_preserve),boost::mem_fn (&CDPV3BookMark::IsUnLockedBookmark));
            bookmarks_to_delete.erase(remove_if(bookmarks_to_delete.begin(),bookmarks_to_delete.end(),boost::mem_fn(&CDPV3BookMark::IsLockedBookmark)),bookmarks_to_delete.end());
            bfiter = bookmarks_to_delete.begin();

            //moving bookmarks from deleted list to preserve which have same timestamp as in preserved list 			
            while(bfiter != bookmarks_to_delete.end())
            {
                if(is_matching_timestamp(bfiter ->c_timestamp,bfiter ->c_sequence,bookmarks_to_preserve))
                {
                    bookmarks_to_preserve.push_back(*bfiter);
                    bookmarks_to_delete.erase(bfiter);
                    bfiter= bookmarks_to_delete.begin();
                }
                else
                    ++bfiter;
            }
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV3DatabaseImpl::is_expired(const CDPV3BookMark & bkmk, const SV_ULONGLONG & tslc_hr)
{
    bool is_expired = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    
    if((tslc_hr - bkmk.c_timestamp) > bkmk.c_lifetime)
        is_expired = true;

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return is_expired;
}

bool CDPV3DatabaseImpl::is_matching_timestamp(const SV_ULONGLONG & ts_to_match,
                                              const SV_ULONGLONG & seq_to_match,
                                              const std::vector<CDPV3BookMark> & bookmarks)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    bool is_matching = false;
    std::vector<CDPV3BookMark>::const_iterator biter = bookmarks.begin();
    std::vector<CDPV3BookMark>::const_iterator bend = bookmarks.end();

    for( ; biter != bend ; ++biter)
    {
        if((biter ->c_timestamp == ts_to_match) && (biter ->c_sequence == seq_to_match))
        {
            is_matching = true;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return is_matching;
}

bool CDPV3DatabaseImpl::is_matching_userbookmark(const std::string & bkmk_to_match,
                                                 const std::vector<std::string> & userbookmarks)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    bool is_matching = false;
    std::vector<std::string>::const_iterator uiter = userbookmarks.begin();
    std::vector<std::string>::const_iterator uend = userbookmarks.end();

    for( ; uiter != uend ; ++uiter)
    {
        if(RegExpMatch((*uiter).c_str(),bkmk_to_match.c_str()))
        {
            is_matching = true;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return is_matching;
}

bool CDPV3DatabaseImpl::fetch_timestamps_to_preserve(const std::vector<CDPV3BookMark> & bookmarks_to_preserve,
                                                     std::vector<CDPV3RecoveryRange> & timestamps_to_preserve)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    do
    {
        for(unsigned int i = 0; i < bookmarks_to_preserve.size(); ++i)
        {
            CDPV3BookMark bkmk = bookmarks_to_preserve[i];
            CDPV3RecoveryRange timerange;

            timerange.c_entryno = 0;
            timerange.c_tsfc = bkmk.c_timestamp;
            timerange.c_tslc = bkmk.c_timestamp;
            timerange.c_tsfcseq = bkmk.c_sequence;
            timerange.c_tslcseq = bkmk.c_sequence;
            timerange.c_granularity = CDP_ONLY_END_POINTS;
            timerange.c_accuracy = bkmk.c_accuracy;
            timerange.c_cxupdatestatus = CS_ADD_PENDING;

            if(find(timestamps_to_preserve.begin(), timestamps_to_preserve.end(),timerange)
                == timestamps_to_preserve.end())
            {
                timestamps_to_preserve.push_back(timerange);
            }
        }

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::fetch_timeranges_in_db(const SV_ULONGLONG & start_ts,
                                               const SV_ULONGLONG & end_ts,
                                               std::vector<CDPV3RecoveryRange> & timeranges_in_db)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());

    std::stringstream query;
    query << "select * from " << CDPV3RECOVERYRANGESTBLNAME 
        << " where c_tsfc >= " 
        << start_ts
        << " and c_tslc < "
        << end_ts 
        << " and c_cxupdatestatus != " << CS_DELETE_PENDING
        << " order by c_tsfc ASC " ;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    while(m_reader.read())
    {
        CDPV3RecoveryRange timerange;

        timerange.c_entryno = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_ENTRYNO_COL);
        timerange.c_tsfc = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFC_COL);
        timerange.c_tslc = m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLC_COL);
        timerange.c_tsfcseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSFCSEQ_COL);
        timerange.c_tslcseq =  m_reader.getint64(CDPV3_RECOVERYRANGESTBL_C_TSLCSEQ_COL);
        timerange.c_granularity = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_GRANULARITY_COL));
        timerange.c_accuracy = static_cast<SV_USHORT> (m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_ACCURACY_COL));
        timerange.c_cxupdatestatus = m_reader.getint(CDPV3_RECOVERYRANGESTBL_C_CXUPDATESTATUS_COL);

        timeranges_in_db.push_back(timerange);
    } 

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s dbname:%s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV3DatabaseImpl::is_already_coalesced(std::vector<CDPV3BookMark> & bookmarks_to_delete,
                                             std::vector<CDPV3RecoveryRange> & timeranges_in_db,
                                             std::vector<CDPV3RecoveryRange> & timestamps_to_preserve)
{
    bool already_coalesced = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    do
    {
        if(!bookmarks_to_delete.empty())
        {
            already_coalesced = false;
            break;
        }

        if(timeranges_in_db.size() > timestamps_to_preserve.size())
        {
            already_coalesced = false;
            break;
        }

        std::vector<CDPV3RecoveryRange>::const_iterator range_iter = timeranges_in_db.begin();
        for( ; range_iter != timeranges_in_db.end(); ++range_iter)
        {
            if((*range_iter).c_granularity == CDP_ANY_POINT_IN_TIME)
            {
                already_coalesced = false;
                break;
            }
        }

    } while(0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return already_coalesced;
}

bool CDPV3DatabaseImpl::fetch_cdptimestamps(const std::vector<CDPV3BookMark> & bkmks, 
                                            std::vector<cdp_timestamp_t> & timestamps_to_keep)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    bool rv = true;
    SV_TIME svtime;
    SV_ULONGLONG ts = 0;
    cdp_timestamp_t cdp_ts;


    do
    {
        for(unsigned int i = 0; i < bkmks.size(); ++i)
        {
            CDPV3BookMark bkmk = bkmks[i];
            ts = bkmk.c_timestamp;

            if(!CDPUtil::ToSVTime(ts,svtime))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                    FUNCTION_NAME, ts);
                rv = false;
                break;
            }

            cdp_ts.year = svtime.wYear - CDP_BASE_YEAR;
            cdp_ts.month = svtime.wMonth;
            cdp_ts.days = svtime.wDay;
            cdp_ts.hours = svtime.wHour;
            cdp_ts.events = bkmk.c_counter;
            timestamps_to_keep.push_back(cdp_ts);
        }

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::delete_coalesced_bookmarks(std::vector<CDPV3BookMark> & bookmarks_to_delete)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());


    std::vector<CDPV3BookMark>::const_iterator bkmk_iter = bookmarks_to_delete.begin();
    for( ; bkmk_iter != bookmarks_to_delete.end(); ++bkmk_iter)
    {

        std::stringstream updatequery;
        updatequery << " update " << CDPV3BOOKMARKSTBLNAME
            << " set c_cxupdatestatus = " 
            << CS_DELETE_PENDING
            << " where c_entryno = " 
            << (*bkmk_iter).c_entryno;

        sqlite3_command update_cmd(*m_con, updatequery.str() );
        update_cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV3DatabaseImpl::delete_coalesced_timeranges(std::vector<CDPV3RecoveryRange> & timeranges_to_delete)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());


    std::vector<CDPV3RecoveryRange>::const_iterator range_iter = timeranges_to_delete.begin();
    for( ; range_iter != timeranges_to_delete.end(); ++range_iter)
    {
        CDPV3RecoveryRange timerange = *range_iter;
        std::stringstream updatequery;
        updatequery << "update " << CDPV3RECOVERYRANGESTBLNAME 
            << " set c_cxupdatestatus = " 
            << CS_DELETE_PENDING
            << " where c_entryno = " 
            << timerange.c_entryno;

        sqlite3_command update_cmd(*m_con, updatequery.str() );
        update_cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV3DatabaseImpl::add_coalesced_timestamps(std::vector<CDPV3RecoveryRange> & timestamps_to_preserve)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());


    std::vector<CDPV3RecoveryRange>::const_iterator range_iter = timestamps_to_preserve.begin();
    for( ; range_iter != timestamps_to_preserve.end(); ++range_iter)
    {
        CDPV3RecoveryRange timerange = *range_iter;
        std::stringstream insertquery;
        insertquery << "insert into " << CDPV3RECOVERYRANGESTBLNAME 
            << " ( c_tsfc, c_tslc, c_tsfcseq, c_tslcseq, c_granularity, c_accuracy, c_cxupdatestatus )"
            << " values(" 
            <<  timerange.c_tsfc << "," 
            <<  timerange.c_tslc << "," 
            <<  timerange.c_tsfcseq << ","
            <<  timerange.c_tslcseq << ","
            <<  timerange.c_granularity << ","
            <<  timerange.c_accuracy << ","
            <<  timerange.c_cxupdatestatus
            << ");" ;

        sqlite3_command insert_cmd(*m_con, insertquery.str() );
        insert_cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());

    return rv;
}



bool CDPV3DatabaseImpl::delete_coalesced_datafiles(const SV_ULONGLONG & coalesce_start_ts, 
                                                   const SV_ULONGLONG &coalesce_end_ts, 
                                                   const std::vector<CDPV3BookMark> & bookmarks_to_keep,
                                                   const std::vector<cdp_timestamp_t> &cdp_timestamps_to_keep)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    do
    {
        cdp_timestamp_t cdp_coalesce_start_ts;
        SV_TIME svtime;
        if(!CDPUtil::ToSVTime(coalesce_start_ts,svtime))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                FUNCTION_NAME, coalesce_start_ts);
            rv = false;
            break;
        }

        cdp_coalesce_start_ts.year = svtime.wYear - CDP_BASE_YEAR;
        cdp_coalesce_start_ts.month = svtime.wMonth;
        cdp_coalesce_start_ts.days = svtime.wDay;
        cdp_coalesce_start_ts.hours = svtime.wHour;
        cdp_coalesce_start_ts.events = 0;

        SV_ULONGLONG ts = coalesce_start_ts;
        for( ; ts < coalesce_end_ts ; ts += HUNDREDS_OF_NANOSEC_IN_HOUR)
        {

            std::string metadatafilepath = cdpv3metadatafile_t::getfilepath(ts, dbdir());
            std::string logfilepath = cdpv3metadatafile_t::getlogpath(ts, dbdir());
            if(!delete_coalesced_datafiles(metadatafilepath,logfilepath,bookmarks_to_keep, cdp_timestamps_to_keep,cdp_coalesce_start_ts))
            {
                rv = false;
                break;
            }	

            if(CDPUtil::QuitRequested())
            {
                rv = false;
                break;
            }
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::delete_coalesced_datafiles(const std::string & metadatapath,
                                                   const std::string & logpath,
                                                   const std::vector<CDPV3BookMark> & bookmarks_to_keep,
                                                   const std::vector<cdp_timestamp_t> & timestamps_to_keep,
                                                   const cdp_timestamp_t & cdp_coalesce_start_ts)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    do
    {

        LocalConfigurator lc;
        bool simulatesparse = lc.SimulateSparse();
        bool track_extra_coalesced_files = lc.TrackExtraCoalescedFiles();
        bool track_coalesced_files = lc.TrackCoalescedFiles();
        
        const std::string dat_pattern = ".dat";
        const std::string deleted_pattern = ".del";

        bool start_new_metadata = false;
        bool keep_bookmark = false;
        cdpv3metadata_t old_metadata;
        cdpv3metadata_t new_metadata;
        std::vector<std::string> filenames_to_delete;
        std::map<SV_UINT, SV_UINT> fileid_map;

        ACE_HANDLE handle = ACE_INVALID_HANDLE;
        SV_OFFSET_TYPE baseoffset = 0;        
        std::string tmp_metadatapath = metadatapath;
        tmp_metadatapath += ".tmp";


        old_metadata.clear();
        new_metadata.clear();

        ACE_stat statbuf;
        if(0 != sv_stat(getLongPathName(metadatapath.c_str()).c_str(),&statbuf))
        {
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s is not yet created?\n", metadatapath.c_str());
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, metadatapath.c_str());
            rv = true;
            break;
        }

        if(!simulatesparse)
        {
            int openmode = O_CREAT | O_WRONLY | O_TRUNC;

#ifdef SV_WINDOWS
            openmode |= (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
#endif

            handle = ACE_OS::open(getLongPathName(tmp_metadatapath.c_str()).c_str(), openmode);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s failed to open %s. error=%d\n",
                    FUNCTION_NAME, tmp_metadatapath.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }
        }

        cdpv3metadatafile_t mdfile(metadatapath);
        SVERROR sv = SVS_OK;

        ofstream coalesced_log;
        if(track_coalesced_files && !simulatesparse)
        {
            coalesced_log.open(logpath.c_str(), ios::app);
            if(!coalesced_log)
            {
                DebugPrintf(SV_LOG_ERROR, "failed to open %s\n", logpath.c_str());
            }
        }

        while((sv = mdfile.read_metadata_asc(old_metadata)) == SVS_OK)
        {
            // if this contains userbookmark and it is to be retained
            // push the existing metadata and start a new one else
            // just append
            // figure out the files to keep
            // figure out the files to delete
            // 
            // delta will be zero
            // fileid will be fileid + baseid
            // baseid will be incremented by no. of files added in this run
            // 

            fileid_map.clear();

            if(!old_metadata.m_users.empty() 
                && retain_bookmark(old_metadata.m_users, bookmarks_to_keep))
            {
                start_new_metadata = true;
                keep_bookmark = true;
            }else
            {
                start_new_metadata = false;
                keep_bookmark = false;
            }

            if(0 == new_metadata.m_tsfc)
            {
                start_new_metadata = true;
            }

            if(0 != new_metadata.m_tsfc && start_new_metadata)
            {
                if(!simulatesparse)
                {
                    if(!write_metadata(tmp_metadatapath,handle, baseoffset, new_metadata))
                    {
                        rv = false;
                        break;
                    }
                }
            }

            if(start_new_metadata)
            {
                new_metadata.clear();
                new_metadata.m_tsfc = old_metadata.m_tsfc;
                new_metadata.m_tsfcseq = old_metadata.m_tsfcseq;
                new_metadata.m_type |= DIFFTYPE_DIFFSYNC_BIT_SET;
            }

            new_metadata.m_tslc = old_metadata.m_tslc;
            new_metadata.m_tslcseq = old_metadata.m_tslcseq;

            if(keep_bookmark)
            {
                new_metadata.m_users.assign(old_metadata.m_users.begin(), 
                    old_metadata.m_users.end());
            }


            for(unsigned int i = 0; i < old_metadata.m_filenames.size(); ++i)
            {
                std::string filename = old_metadata.m_filenames[i];

                std::vector<string>::const_iterator fiter = find(new_metadata.m_filenames.begin(),
                    new_metadata.m_filenames.end(),filename);

                if (fiter == new_metadata.m_filenames.end())
                {
                    if(find(filenames_to_delete.begin(), 
                        filenames_to_delete.end(), filename) == filenames_to_delete.end())
                    {
                        if(retain_filename(filename, timestamps_to_keep,cdp_coalesce_start_ts, 
                            track_extra_coalesced_files))
                        {
                            new_metadata.m_filenames.push_back(filename);
                            new_metadata.m_baseoffsets.push_back(old_metadata.m_baseoffsets[i]);
                            new_metadata.m_byteswritten.push_back(old_metadata.m_byteswritten[i]);
                            fileid_map.insert(std::make_pair(i,new_metadata.m_filenames.size() -1));
                        }else
                        {
                            filenames_to_delete.push_back(filename);
                        }
                    }

                } else
                {
                    fileid_map.insert(std::make_pair(i, fiter - new_metadata.m_filenames.begin()));
                }
            }

            for(unsigned int i = 0; i < old_metadata.m_drtds.size(); ++i)
            {
                cdp_drtdv2_t old_drtd = old_metadata.m_drtds[i];
                std::map<SV_UINT,SV_UINT>::const_iterator fiter =
                    fileid_map.find(old_drtd.get_fileid());
                if(fiter != fileid_map.end())
                {
                    cdp_drtdv2_t new_drtd(old_drtd.get_length(),
                        old_drtd.get_volume_offset(),
                        old_drtd.get_file_offset(),
                        0,
                        0,
                        fiter ->second);
                    new_metadata.m_drtds.push_back(new_drtd);
                }
            }

        }

        mdfile.close();
        if(sv.failed())
        {
            rv = false;
        }

        if(0 != new_metadata.m_tsfc)
        {
            if(!simulatesparse)
            {
                if(!write_metadata(tmp_metadatapath,handle, baseoffset, new_metadata))
                {
                    rv = false;
                }
            }
            new_metadata.clear();
        }

        if(ACE_INVALID_HANDLE != handle)
        {
#ifndef SV_WINDOWS
            if(ACE_OS::fsync(handle) == -1)
        {
                DebugPrintf(SV_LOG_ERROR, 
                    "Flush buffers failed for %s with error %d.\n",
                    tmp_metadatapath.c_str(),
                    ACE_OS::last_error());
            rv = false;
            }
#endif
            ACE_OS::close(handle);
        }

        if(!rv)
        {
            ACE_OS::unlink(getLongPathName(tmp_metadatapath.c_str()).c_str());
            break;
        }


        if(!simulatesparse)
        {
        if(0 != ACE_OS::rename(getLongPathName(tmp_metadatapath.c_str()).c_str(),
            getLongPathName(metadatapath.c_str()).c_str()))
        {
            rv = false;
            break;
        }
        }

        std::string deletion_time = boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time());

        // delete the data files to be deleted
        for(unsigned int i = 0 ; i < filenames_to_delete.size(); ++i)
        {
            std::string datafilename = m_dbdir;
            datafilename += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            datafilename += filenames_to_delete[i];

			if(!simulatesparse)
			{
                if(track_coalesced_files && coalesced_log.is_open())
                {
                    coalesced_log << deletion_time << "," 
                        << basename_r(datafilename.c_str()) << "," 
                        << File::GetSizeOnDisk(datafilename) << "\n";
                }

				if((0 != ACE_OS::unlink(getLongPathName(datafilename.c_str()).c_str())) 
					&& (ACE_OS::last_error() != ENOENT))
				{
					stringstream l_stdfatal;
					l_stdfatal << "Error detected  " << "in Function:" 
						<< FUNCTION_NAME << " @ Line:" << LINE_NO
						<< " FILE:" << FILE_NAME << "\n\n"
						<< " Error occured on unlink of data file: " 
						<< datafilename << "\n"
						<< " Error Code : " << Error::Code() << "\n"
						<< " Error Message:" << Error::Msg() << "\n"
						<< " Please delete the file manually\n";

					DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());		
				}
			} 
			else
			{

				std::string coalesced_datafilename = datafilename;
                coalesced_datafilename.replace(coalesced_datafilename.find(dat_pattern), dat_pattern.size(),deleted_pattern);

				if((0 != ACE_OS::rename(getLongPathName(datafilename.c_str()).c_str(),
					getLongPathName(coalesced_datafilename.c_str()).c_str()))
					&& (ACE_OS::last_error() != ENOENT))
				{
					stringstream l_stdfatal;
					l_stdfatal << "Error detected  " << "in Function:" 
						<< FUNCTION_NAME << " @ Line:" << LINE_NO
						<< " FILE:" << FILE_NAME << "\n\n"
						<< " Error occured on rename of data file: " 
						<< datafilename << "\n"
						<< " to "
						<< coalesced_datafilename << "\n"
						<< " Error Code : " << Error::Code() << "\n"
						<< " Error Message:" << Error::Msg() << "\n"
						<< " Please rename the file manually\n";

					DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());	
				}
			}
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::parse_filename(const std::string & filename,
                                       unsigned long long & seq,
                                       cdp_timestamp_t & start_ts,
                                       cdp_timestamp_t & end_ts,
                                       bool & isoverlap)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filename:%s\n",
        FUNCTION_NAME, filename.c_str());

    try
    {
        // parse the v3 data file name whose format is as below
        // cdpv30_<end time><end bookmark>_<file type(1 - overlap/ 0 - first write)>_
        //        <start time><start bookmark>_<sequence no>.dat
        // example: cdpv30_010422080010_1_010422080010_1234.dat
        // See bug# 11826 for more details

        std::string::size_type indx = filename.find('_');
        end_ts.year = boost::lexical_cast<unsigned int>(filename.substr(indx+1, 2));
        end_ts.month = boost::lexical_cast<unsigned int>(filename.substr(indx+3, 2));
        end_ts.days = boost::lexical_cast<unsigned int>(filename.substr(indx+5, 2));
        end_ts.hours = boost::lexical_cast<unsigned int>(filename.substr(indx+7, 2));
        end_ts.events = boost::lexical_cast<unsigned int>(filename.substr(indx+9, 4));

        indx = filename.find('_',indx+1);
        isoverlap = boost::lexical_cast<bool>(filename.substr(indx+1, 1));

        indx = filename.find('_',indx+1);
        start_ts.year = boost::lexical_cast<unsigned int>(filename.substr(indx+1, 2));
        start_ts.month = boost::lexical_cast<unsigned int>(filename.substr(indx+3, 2));
        start_ts.days = boost::lexical_cast<unsigned int>(filename.substr(indx+5, 2));
        start_ts.hours = boost::lexical_cast<unsigned int>(filename.substr(indx+7, 2));
        start_ts.events = boost::lexical_cast<unsigned int>(filename.substr(indx+9, 4));

        indx = filename.find('_',indx+1);
        std::string::size_type indx2 = filename.find('.',indx+1);
        seq = boost::lexical_cast<unsigned long long>(filename.substr(indx+1, indx2-(indx+1)));
    }
    catch(std::exception& ex)
    {
        std::stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while parsing filename:" 
            << filename << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s filename:%s\n",
        FUNCTION_NAME, filename.c_str());
    return rv;
}



bool CDPV3DatabaseImpl::retain_bookmark(const UserTags & bookmarks_to_match, 
                                        const std::vector<CDPV3BookMark> & bookmarks_to_keep)
{
    bool found = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    UserTagsConstIterator uiter = bookmarks_to_match.begin();
    UserTagsConstIterator uend = bookmarks_to_match.end();
    std::vector<CDPV3BookMark>::const_iterator biter = bookmarks_to_keep.begin();
    std::vector<CDPV3BookMark>::const_iterator bend = bookmarks_to_keep.end();

    for ( ; uiter != uend; ++uiter)
    {
        SvMarkerPtr pMarker = *uiter;
        biter = bookmarks_to_keep.begin();
        for( ; biter != bend; ++biter)
        {

            if( (biter ->c_apptype == pMarker -> TagType()) &&
                (biter ->c_value == std::string(pMarker -> Tag().get())))
            {
                found = true;
                break;
            }
        }

        if(found)
            break;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return found;
}

void CDPV3DatabaseImpl::TrackExtraCoalescedFiles(std::string const & trackingPath, std::string const & fileName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::ofstream out;

    out.open(getLongPathName(trackingPath.c_str()).c_str(), std::ios::app);
    if (!out) {
        DebugPrintf(SV_LOG_DEBUG, "unable to open %s error = %d\n", trackingPath.c_str(), errno);
        return;
    }

    out << fileName << '\n';
    DebugPrintf( SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME ) ;
}


bool CDPV3DatabaseImpl::retain_filename(const std::string & filename, 
                                      const std::vector<cdp_timestamp_t> & timestamps_to_keep,
                                      const cdp_timestamp_t & cdp_coalesce_start_ts,
                                      bool & track_extra_coalesced_files)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s %s\n",FUNCTION_NAME, filename.c_str());
    bool retain = false;
    SV_ULONGLONG seq = 0;
    cdp_timestamp_t start_ts;
    cdp_timestamp_t end_ts;
    bool isoverlap = false;
    cdp_timestamp_t ts_to_match;
    bool would_have_deleted_without_patch = false;

    do
    {
        if(!parse_filename(filename, seq, start_ts, end_ts, isoverlap))
        {
            retain = false;
            break;
        }

        if((start_ts < cdp_coalesce_start_ts) && !(end_ts < cdp_coalesce_start_ts))
        {

            would_have_deleted_without_patch = true;
            retain = true;
        }

        for (unsigned int i = 0; i < timestamps_to_keep.size(); ++i)
        {
            ts_to_match = timestamps_to_keep[i];
            if((start_ts < ts_to_match) && !(end_ts < ts_to_match))
            {
                retain = true;
                would_have_deleted_without_patch = false;
                break;
            }
        }

    } while (0);

    if(track_extra_coalesced_files && would_have_deleted_without_patch)
    {
        std::string trackingDir(dbdir());
        std::string trackingFileName("extra_coalesced_files.txt");
        std::string trackingPath = trackingDir;
        trackingPath += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        trackingPath += trackingFileName;

        TrackExtraCoalescedFiles(trackingPath, filename);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return retain;
}


bool CDPV3DatabaseImpl::write_metadata(const std::string & metadata_filepath,
                                       std::vector<cdpv3metadata_t> & metadatas)
{

    bool rv = true;
    ACE_HANDLE handle = ACE_INVALID_HANDLE;
    SV_OFFSET_TYPE baseoffset = -1;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s metadata_filepath:%s\n",
        FUNCTION_NAME, metadata_filepath.c_str());

    do
    {

        int openmode = O_CREAT | O_WRONLY | O_TRUNC;
#ifdef SV_WINDOWS
        openmode |= (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH );
#endif

        handle = ACE_OS::open(getLongPathName(metadata_filepath.c_str()).c_str(), openmode);

        if(ACE_INVALID_HANDLE == handle)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s failed to open %s. error=%d\n",
                FUNCTION_NAME, metadata_filepath.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        baseoffset = 0; 

        for(unsigned int i = 0; i < metadatas.size(); ++i)
        {
            if(!write_metadata(metadata_filepath,handle, baseoffset, metadatas[i]))
            {
                rv = false;
                break;
            }
        }

#ifndef SV_WINDOWS
        if(ACE_OS::fsync(handle) == -1)
        {
            DebugPrintf(SV_LOG_ERROR, 
                "Flush buffers failed for %s with error %d.\n",
                metadata_filepath.c_str(),
                ACE_OS::last_error());
            rv = false;
        }
#endif
    } while (0);

    if(ACE_INVALID_HANDLE != handle)
    {
        ACE_OS::close(handle);
    }

    if(!rv)
    {
        ACE_OS::unlink(getLongPathName(metadata_filepath.c_str()).c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s metadata_filepath:%s\n",
        FUNCTION_NAME, metadata_filepath.c_str());
    return rv;
}

bool CDPV3DatabaseImpl::write_metadata(const std::string & metadata_filepath,
                                       ACE_HANDLE &handle,
                                       SV_OFFSET_TYPE & baseoffset,
                                       const cdpv3metadata_t & cow_metadata)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s handle metadatafile: %s\n",
        FUNCTION_NAME, metadata_filepath.c_str());


    do
    {
        //
        // local variables
        //
        SVD_PREFIX pfx;
        SV_UINT buflen = 0;
        SV_UINT skip_bytes = 0;
        SV_UINT delta = 0;
        SV_UINT i = 0;

        char * buffer = 0;
        SV_ULONG curpos = 0;
		LocalConfigurator localConfigurator;
		SV_ULONGLONG sector_size = localConfigurator.getVxAlignmentSize();

        // get length of metadata buffer
        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_TIMESTAMP_V2_SIZE;
        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_TIMESTAMP_V2_SIZE;
        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_OTHR_INFO_SIZE;

        if(!cow_metadata.m_filenames.empty())
        {
            buflen += SVD_PREFIX_SIZE;
            buflen += (cow_metadata.m_filenames.size() * SVD_FOST_INFO_SIZE);
        }

        if(!cow_metadata.m_drtds.empty())
        {
            buflen += SVD_PREFIX_SIZE;
            buflen += (cow_metadata.m_drtds.size() * SVD_CDPDRTD_SIZE);
        }

        for( i = 0 ; i < cow_metadata.m_users.size(); ++i)
        {
            buflen += SVD_PREFIX_SIZE;
            buflen += cow_metadata.m_users[i] -> RawLength();
        }

        buflen += SVD_PREFIX_SIZE; // skip bytes

        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_SOST_SIZE;

        delta = (buflen % sector_size);
        if(delta)
            skip_bytes = sector_size - delta;

        buflen += skip_bytes;

        SharedAlignedBuffer alignedbuf(buflen, sector_size);
        buffer = alignedbuf.Get();
		//To find size of buffer
		int size = alignedbuf.Size();

        // fill the tfv2
        SVD_TIME_STAMP_V2 tfv2;
        memset(&tfv2,0, SVD_TIMESTAMP_V2_SIZE);
        tfv2.TimeInHundNanoSecondsFromJan1601 = cow_metadata.m_tsfc;
        tfv2.SequenceNumber = cow_metadata.m_tsfcseq;

        FILL_PREFIX(pfx, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2, 1, 0);
		inm_memcpy_s(buffer + curpos, (size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (size - curpos),&tfv2, SVD_TIMESTAMP_V2_SIZE);
        curpos += SVD_TIMESTAMP_V2_SIZE;

        // fill the tlv2
        SVD_TIME_STAMP_V2 tlv2;
        memset(&tlv2,0, SVD_TIMESTAMP_V2_SIZE);
        tlv2.TimeInHundNanoSecondsFromJan1601 = cow_metadata.m_tslc;
        tlv2.SequenceNumber = cow_metadata.m_tslcseq;

        FILL_PREFIX(pfx, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2, 1, 0);
		inm_memcpy_s(buffer + curpos, (size - curpos),&pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (size - curpos),&tlv2, SVD_TIMESTAMP_V2_SIZE);
        curpos += SVD_TIMESTAMP_V2_SIZE;

        // fill othr info
        FILL_PREFIX(pfx, SVD_TAG_OTHR, 1, 0);
		inm_memcpy_s(buffer + curpos, (size - curpos),&pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (size - curpos),&(cow_metadata.m_type), SVD_OTHR_INFO_SIZE);
        curpos += SVD_OTHR_INFO_SIZE;

        // fill cdp data files
        if(!cow_metadata.m_filenames.empty())
        {
            FILL_PREFIX(pfx, SVD_TAG_FOST, cow_metadata.m_filenames.size(), 0);
			inm_memcpy_s(buffer + curpos, (size - curpos),&pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            for( i = 0; i < cow_metadata.m_filenames.size(); ++i)
            {
                SVD_FOST_INFO fost;
                memset(fost.filename, '\0', SVD_FOST_FNAME_SIZE);
                inm_strncpy_s(fost.filename, ARRAYSIZE(fost.filename), cow_metadata.m_filenames[i].c_str(), SVD_FOST_FNAME_SIZE);
                fost.startoffset = cow_metadata.m_baseoffsets[i];
				inm_memcpy_s(buffer + curpos, (size - curpos),&fost, SVD_FOST_INFO_SIZE);
                curpos += SVD_FOST_INFO_SIZE;
            }
        }

        // fill drtds
        if(!cow_metadata.m_drtds.empty())
        {
            FILL_PREFIX(pfx, SVD_TAG_CDPD, cow_metadata.m_drtds.size(), 0);
			inm_memcpy_s(buffer + curpos, (size - curpos),&pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            for( i = 0; i < cow_metadata.m_drtds.size(); ++i)
            {
                SVD_CDPDRTD drtd;
                drtd.voloffset = cow_metadata.m_drtds[i].get_volume_offset();
                drtd.length = cow_metadata.m_drtds[i].get_length();
                drtd.fileid = cow_metadata.m_drtds[i].get_fileid();
                drtd.uiSequenceNumberDelta = cow_metadata.m_drtds[i].get_seqdelta();
                drtd.uiTimeDelta = cow_metadata.m_drtds[i].get_timedelta();
				inm_memcpy_s(buffer + curpos, (size - curpos),&drtd, SVD_CDPDRTD_SIZE);
                curpos += SVD_CDPDRTD_SIZE;
            }
        }

        // fill bookmarks
        for( i = 0 ; i < cow_metadata.m_users.size(); ++i)
        {
            FILL_PREFIX(pfx, SVD_TAG_USER, 1,  cow_metadata.m_users[i] -> RawLength());
			inm_memcpy_s(buffer + curpos, (size - curpos),&pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
			inm_memcpy_s(buffer + curpos, (size - curpos), cow_metadata.m_users[i]->RawData(),
                cow_metadata.m_users[i] -> RawLength());
            curpos += cow_metadata.m_users[i] -> RawLength();
        }

        // fill skip bytes
        FILL_PREFIX(pfx, SVD_TAG_SKIP, skip_bytes, 0);
		inm_memcpy_s(buffer + curpos, (size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
        curpos += skip_bytes;

        // fill sost information
        FILL_PREFIX(pfx, SVD_TAG_SOST, 1, 0);
		inm_memcpy_s(buffer + curpos, (size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (size - curpos), &baseoffset, SVD_SOST_SIZE);
        curpos += SVD_SOST_SIZE;


        // write metadata
        unsigned int byteswrote = ACE_OS::write(handle, buffer, buflen);
        if(byteswrote != buflen)
        {
            DebugPrintf(SV_LOG_ERROR, 
                "FUNCTION:%s write to file %s failed. byteswrote: %d, expected:%d. error=%d\n",
                FUNCTION_NAME, metadata_filepath.c_str(), byteswrote, buflen, ACE_OS::last_error());

            rv = false;
            break;
        }

        baseoffset +=  curpos;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s handle metadata_filepath:%s\n",
        FUNCTION_NAME, metadata_filepath.c_str());
    return rv;
}

/*	Vaildates the retention data base,
*      a) Detect deletion of data files - Verifies that all the .dat files referenced by .mdat files are present.
*	   b) Detect deletion of mdat files - Verify that for all the .dat files present, a corresponding .mdat file is present.
*	   c) Verify recovery points - Verify that no necessary .dat file is deleted as part of coalesce.
*      d) Verify that no stale files are present.
*/
bool CDPV3DatabaseImpl::validate()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	LocalConfigurator localConfigurator;

	DebugPrintf(SV_LOG_INFO,"\nVerifying that all the data files referenced by metadata files are present ");

    if(!verify_datafiles_used_in_metadata_exist())
	{
		rv = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO," OK\n");
	}
	
	DebugPrintf(SV_LOG_INFO,"\nVerifying that all metadata files exists ");

    if(!verify_metadatafiles_for_all_datafiles_exist())
	{
		rv = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO," OK\n");
	}

	DebugPrintf(SV_LOG_INFO,"\nVerifying if stale files are present ");

	std::list<std::string> stale_files;
	stale_files.clear();
	get_listof_stalefiles(stale_files);

	if(!stale_files.empty())
	{
		rv = false;
		std::list<std::string>::iterator it = stale_files.begin();
		while(it!=stale_files.end())
		{
			DebugPrintf(SV_LOG_INFO,"\n    Error : Stale file %s exists\n",it->c_str());
			it++;
		}

	}
	else
	{
		DebugPrintf(SV_LOG_INFO," OK\n");
	}

    if(localConfigurator.TrackCoalescedFiles())
					{
        DebugPrintf(SV_LOG_INFO,"\nVerifying recovery points ");
        if(!verify_recovery_points())
						{
							rv = false;
        }	else
					{
            DebugPrintf(SV_LOG_INFO," OK\n");
			}
            }
			
			if(rv)
			{
        DebugPrintf(SV_LOG_INFO,"\n\nRetention Data Verified - OK\n");
			}
			else
			{
        DebugPrintf(SV_LOG_INFO,"\n\nRetention Data Verification - FAILED\n");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;

        }



//For each meta data file in the retention directory, obtain the list of
//dat files referred and the size of each file.
//The same dat file can be reffered multiple times with in the metadata file.
//The last entry should be taken for the total size of the dat file.
bool CDPV3DatabaseImpl::verify_datafiles_used_in_metadata_exist()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

	bool rv = true;
	SV_ULONGLONG tsfc_inhrs;
    SV_ULONGLONG tslc_inhrs;
	SV_ULONGLONG tsfc, tslc;

	getrecoveryrange(tsfc,tslc);
	CDPUtil::ToTimestampinHrs(tsfc,tsfc_inhrs);
	CDPUtil::ToTimestampinHrs(tslc,tslc_inhrs);

	SV_ULONGLONG metadatafiletime = tslc_inhrs;
	
	while(metadatafiletime >= tsfc_inhrs)
	{
		std::string metadatapathname = cdpv3metadatafile_t::getfilepath(metadatafiletime,dbdir());
		
		cdpv3metadatafile_t mdfile(metadatapathname);
		cdpv3metadata_t metadata;
		std::map<std::string,SV_ULONGLONG> datafiles; //Map file for storing data files and corresponding size reffered in a metadata file.
		datafiles.clear();
		while(mdfile.read_metadata_asc(metadata) == SVS_OK)
		{
            for(unsigned int i = 0;i<metadata.m_filenames.size();i++)
			{
				//Add the file name and the size(offset + bytes written) to the map.
				//If the file has multiple entries, the previous entry will be overwritten by the latest one.
                //Verify increasing offsets.
                if(datafiles.find(metadata.m_filenames[i]) == datafiles.end() || datafiles[metadata.m_filenames[i]] == metadata.m_baseoffsets[i])
                {
				datafiles[metadata.m_filenames[i]] = metadata.m_baseoffsets[i]+metadata.m_byteswritten[i];
                }
                else
                {
                    rv = false;
                    DebugPrintf(SV_LOG_INFO,"\n    Error: Inconsistent offsets for %s Actual offset/Offset from metadata - " ULLSPEC "/" ULLSPEC "\n",
                        metadata.m_filenames[i].c_str(),metadata.m_baseoffsets[i],datafiles[metadata.m_filenames[i]]);
                    datafiles[metadata.m_filenames[i]] += metadata.m_byteswritten[i];
                }	
            }
				
            std::map<SV_UINT,SV_ULONGLONG> drtdlength; //map for storing data files and the total drtd length.
            drtdlength.clear();
            for (unsigned int i = 0;  i < metadata.m_drtds.size() ; ++i)
            {
                if(drtdlength.find(metadata.m_drtds[i].get_fileid()) == drtdlength.end()||drtdlength[metadata.m_drtds[i].get_fileid()] == metadata.m_drtds[i].get_file_offset())
                {
                    drtdlength[metadata.m_drtds[i].get_fileid()] = metadata.m_drtds[i].get_file_offset() + metadata.m_drtds[i].get_length();
                }
                else
                {
                    rv = false;
                    DebugPrintf(SV_LOG_INFO,"\n    Error: Inconsistent offsets for drtds in file %s Actual value/Expected value - " ULLSPEC "/" ULLSPEC "\n",
                        metadata.m_filenames[metadata.m_drtds[i].get_fileid()].c_str(),metadata.m_drtds[i].get_file_offset(),drtdlength[metadata.m_drtds[i].get_fileid()]);
                    drtdlength[metadata.m_drtds[i].get_fileid()] = metadata.m_drtds[i].get_file_offset() + metadata.m_drtds[i].get_length();				
			}
		}
        }

		//Verify that the dat files exist and the file size matches with the values obtained from metadata.
		std::map<std::string,SV_ULONGLONG>::iterator it = datafiles.begin();
		while(it != datafiles.end())
		{
			std::string datafilename = dbdir();
			datafilename += ACE_DIRECTORY_SEPARATOR_STR_A;
			datafilename += it->first;
			ACE_stat statdata;

			if(sv_stat(getLongPathName(datafilename.c_str()).c_str(),&statdata) == 0)
			{

				if(it->second!=statdata.st_size)
				{
					rv = false;
					DebugPrintf(SV_LOG_INFO,"\n    Error: Size not matching for file %s Actual size/Size in metadata - " ULLSPEC "/" ULLSPEC "\n",datafilename.c_str(),statdata.st_size,it->second);
				}
			}
			else
			{
				rv = false;
				DebugPrintf(SV_LOG_INFO,"\n    Error: File does not exist - %s",datafilename.c_str());
			}
			it++;
		}
        metadatafiletime -= HUNDREDS_OF_NANOSEC_IN_HOUR;
	}

    DebugPrintf(SV_LOG_DEBUG,"\nEXITED:%s\n",FUNCTION_NAME);
	return rv;
}

//For each dat file in the retention directory, get the current time stamp and
//verify that the corresponding metadata file exists.
bool CDPV3DatabaseImpl::verify_metadatafiles_for_all_datafiles_exist()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
	try
	{
		do
		{
			// open directory
			ACE_DIR *dirp = sv_opendir(dbdir().c_str());

			if (dirp == NULL)
			{
				int lasterror = ACE_OS::last_error();
				DebugPrintf(SV_LOG_ERROR, 
						"\n%s encountered error %d while reading from %s\n",
						FUNCTION_NAME, lasterror, dbdir().c_str()); 
				rv = false;
				break;
			}

			struct ACE_DIRENT *dp = NULL;
			std::set<std::string> mdfilenames;

			do
			{
				ACE_OS::last_error(0);
				// get directory entry
				if ((dp = ACE_OS::readdir(dirp)) != NULL)
				{
					// find a matching entry for dat files.
					const std::string datfiles = "*.dat";
					if(RegExpMatch(datfiles.c_str(), ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
					{
						std::string filename = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
						std::string::size_type indx = filename.find('_');
						
						//Get the current time stamp from the file name. This can be mapped to the corresponding metadata file.
						stringstream fname_str;
						fname_str << CDPV3_MD_PREFIX  
							<< "y" << boost::lexical_cast<unsigned int>(filename.substr(indx+1, 2))
							<< "m" << boost::lexical_cast<unsigned int>(filename.substr(indx+3, 2))
							<< "d" << boost::lexical_cast<unsigned int>(filename.substr(indx+5, 2))
							<< "h" << boost::lexical_cast<unsigned int>(filename.substr(indx+7, 2))
							<< CDPV3_MD_SUFFIX ;

						if(mdfilenames.find(fname_str.str()) == mdfilenames.end())
						{
							mdfilenames.insert(fname_str.str());
						}
					}
				}

			} while(dp);

			// close directory
			ACE_OS::closedir(dirp);

			//For all files present in the mdfilenames verify if it exists?
			std::set<std::string>::iterator it = mdfilenames.begin();
			while(it!=mdfilenames.end())
			{
				std::string mdfilename = dbdir();
				mdfilename += ACE_DIRECTORY_SEPARATOR_STR_A;
				mdfilename += *it;
				ACE_stat statdata;
				if(sv_stat(getLongPathName(mdfilename.c_str()).c_str(),&statdata) != 0)
				{
					rv = false;
					DebugPrintf(SV_LOG_INFO,"\n    Error: File does not exist - %s",mdfilename.c_str());
				}
				it++;
			}

		} while(false);

	} 
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), dbdir().c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), dbdir().c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, dbdir().c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
	return rv;
}

bool CDPV3DatabaseImpl::verify_recovery_points()
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    try
    {

        do
        {

            SV_ULONGLONG recovery_start_ts = 0;
            SV_ULONGLONG recovery_end_ts = 0;
            std::vector<CDPV3RecoveryRange> timeranges_in_db;
            std::vector<CDPV3BookMark> bookmarks_in_db;

            std::set<CDPV3RecoveryRange> unusableranges;
            std::set<CDPV3RecoveryRange> unusablepoints; 
            std::set<CDPV3BookMark> unusablebookmarks;
            std::set<std::string> unexpected_deletion_logfiles;
            std::set<std::string> missing_datafiles;


            getrecoveryrange(recovery_start_ts,recovery_end_ts);

            connect();

            if(!fetch_timeranges_in_db(recovery_start_ts,recovery_end_ts,timeranges_in_db))
            {
                rv = false;
                break;
            }

            if(!fetch_bookmarks_in_timerange(recovery_start_ts,recovery_end_ts, bookmarks_in_db))
            {
                rv = false;
                break;
            }

            shutdown();

            detect_unusable_anypointrecovery_ranges(timeranges_in_db,unusableranges,unexpected_deletion_logfiles);
            detect_unusable_recoverypoints(recovery_start_ts, recovery_end_ts,timeranges_in_db,bookmarks_in_db, unusablepoints, unusablebookmarks, missing_datafiles);

            if(!unexpected_deletion_logfiles.empty())
            {
                rv = false;

                DebugPrintf(SV_LOG_INFO,
                    "\n    Error: unexpected deletion log(s) for any point recovery time range:");

                std::set<std::string>::iterator it = unexpected_deletion_logfiles.begin();
                while(it != unexpected_deletion_logfiles.end())
                {
                    DebugPrintf(SV_LOG_INFO, "\n          %s", (*it).c_str()); 
                    ++it;
                }
            }


            if(!missing_datafiles.empty())
            {
                rv = false;

                DebugPrintf(SV_LOG_INFO,
                    "\n    Error: unexpected deletion of following data files:");
                std::set<std::string>::iterator it = missing_datafiles.begin();
                while(it != missing_datafiles.end())
                {
                    DebugPrintf(SV_LOG_INFO, "\n          %s", (*it).c_str());
                    ++it;
                }
            }

            if(!unusableranges.empty())
            {
                rv = false;

                DebugPrintf(SV_LOG_INFO,"\n    Unusable recovery ranges :\n");
                std::set<CDPV3RecoveryRange>::iterator it = unusableranges.begin();
                while(it != unusableranges.end())
                {
                    std::string tsfc_displaytime, tslc_displaytime;
                    if(!CDPUtil::ToDisplayTimeOnConsole((*it).c_tsfc,tsfc_displaytime))
                    {
                        DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                            FUNCTION_NAME, (*it).c_tsfc);
                        ++it;
                        continue;
                    }

                    if(!CDPUtil::ToDisplayTimeOnConsole((*it).c_tslc,tslc_displaytime))
                    {
                        DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                            FUNCTION_NAME, (*it).c_tslc);
                        ++it;
                        continue;
                    }

                    DebugPrintf(SV_LOG_INFO,"\n     %s to %s",
                        tsfc_displaytime.c_str(), tslc_displaytime.c_str());
                    ++it;
                }
            }



            if(!unusablepoints.empty())
            {
                rv = false;

                DebugPrintf(SV_LOG_INFO,"\n    Unusable recovery timestamp(s) :\n");
                std::set<CDPV3RecoveryRange>::iterator it = unusablepoints.begin();
                while(it != unusablepoints.end())
                {
                    std::string displaytime;
                    if(!CDPUtil::ToDisplayTimeOnConsole((*it).c_tsfc,displaytime))
                    {
                        DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                            FUNCTION_NAME, (*it).c_tsfc);
                        ++it;
                        continue;
                    }

                    DebugPrintf(SV_LOG_INFO,"\n    %s ", displaytime.c_str());
                    ++it;
                }
            }

            if(!unusablebookmarks.empty())
            {
                rv = false;
                bool namevalueformat_true = true;
                DebugPrintf(SV_LOG_INFO,"\n    Unusable bookmark(s) :\n");
                CDPUtil::displaybookmarks(unusablebookmarks, namevalueformat_true);
            }


        } while(0);

    } 
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, ce.what(), dbdir().c_str());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, e.what(), dbdir().c_str());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception for %s.\n",
            FUNCTION_NAME, dbdir().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

void CDPV3DatabaseImpl::detect_unusable_anypointrecovery_ranges(
    const std::vector<CDPV3RecoveryRange> & timeranges_to_check,
    std::set<CDPV3RecoveryRange> & unusableranges,
    std::set<std::string> & unexpected_deletion_logfiles)
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    for(size_t i = 0; i < timeranges_to_check.size() ; ++i)
    {
        if(CDP_ANY_POINT_IN_TIME == timeranges_to_check[i].c_granularity) 
        {
            std::string deletion_log = cdpv3metadatafile_t::getlogpath(timeranges_to_check[i].c_tsfc,  dbdir());
            ACE_stat statbuf;
            if(sv_stat(getLongPathName(deletion_log.c_str()).c_str(),&statbuf) == 0)
            {
                unusableranges.insert(timeranges_to_check[i]);
                unexpected_deletion_logfiles.insert(deletion_log);
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
}


void CDPV3DatabaseImpl::detect_unusable_recoverypoints(
    const SV_ULONGLONG & recovery_start_ts,
    const SV_ULONGLONG & recovery_end_ts,
    const std::vector<CDPV3RecoveryRange> & timeranges_to_check,
    const std::vector<CDPV3BookMark> & bookmarks_to_check,
    std::set<CDPV3RecoveryRange> & unusablepoints, 
    std::set<CDPV3BookMark> & unusablebookmarks,
    std::set<std::string> &missing_datafiles)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    for( SV_ULONGLONG  ts = recovery_start_ts; ts < recovery_end_ts ; ts += HUNDREDS_OF_NANOSEC_IN_HOUR)
    {

        std::string logfilepath = cdpv3metadatafile_t::getlogpath(ts,  dbdir());
        //Read the logfilepath and get the names of files that are deleted.
        ifstream coalescedlogFile(logfilepath.c_str());

        if(coalescedlogFile.is_open())
        {
            char data[512];
            while(!coalescedlogFile.eof())
            {
                coalescedlogFile.getline(data,512);
                std::string deleteddatfile = data;
                if(deleteddatfile.empty())
                    break;
                //The format of the string is <date>,<deleted dat file>,<size>
                //we are interested in the <deleted dat file>, so truncate rest of the sting.
                deleteddatfile.erase(0,deleteddatfile.find(",")+1); //Delete all the contents before the file name.
                deleteddatfile.erase(deleteddatfile.find(",")); //Delete all the contents after the file name.

                find_affected_recoverypoints(deleteddatfile,
                    timeranges_to_check,
                    bookmarks_to_check,
                    unusablepoints,
                    unusablebookmarks,
                    missing_datafiles);
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
}


void CDPV3DatabaseImpl::find_affected_recoverypoints(const std::string & deleteddatfile,
                                                     const std::vector<CDPV3RecoveryRange> & timeranges_to_check,
                                                     const std::vector<CDPV3BookMark> & bookmarks_to_check,
                                                     std::set<CDPV3RecoveryRange> & unusablepoints,
                                                     std::set<CDPV3BookMark> &unusablebookmarks,
                                                     std::set<std::string> &missing_datafiles)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s %s\n",FUNCTION_NAME,deleteddatfile.c_str());

    do
    {
        SV_ULONGLONG seq = 0;
        cdp_timestamp_t start_ts;
        cdp_timestamp_t end_ts;
        cdp_timestamp_t cdp_ts_to_check;
        bool isoverlap = false;

        if(!parse_filename(deleteddatfile, seq, start_ts, end_ts, isoverlap))
        {
            break;
        }


        for(size_t i = 0; i < timeranges_to_check.size() ; ++i)
        {

            // check if a book mark exists for the same time range.
            std::vector<CDPV3BookMark>::const_iterator bookmark_it = bookmarks_to_check.begin();
            bool bookmark_found = false;
            bool ts_valid = false;

            while(bookmark_it != bookmarks_to_check.end())
            {
                if(bookmark_it->c_timestamp == (timeranges_to_check[i]).c_tsfc)
                {
                    bookmark_found = true;
                    break;
                }
                bookmark_it++;
            }

            if(bookmark_found)
            { 
                ts_valid = fetch_cdptimestamp((*bookmark_it), cdp_ts_to_check);
            } else
            {
                ts_valid = fetch_cdptimestamp(timeranges_to_check[i], cdp_ts_to_check);
            }

            if(ts_valid)
            {
                if((start_ts < cdp_ts_to_check) && !(end_ts < cdp_ts_to_check))
                {
                    unusablepoints.insert(timeranges_to_check[i]);
                    missing_datafiles.insert(deleteddatfile);
                }
            }
        }

        for(size_t i = 0; i < bookmarks_to_check.size() ; ++i)
        {

            if(fetch_cdptimestamp(bookmarks_to_check[i], cdp_ts_to_check))
            {
                if((start_ts < cdp_ts_to_check) && !(end_ts < cdp_ts_to_check))
                {
                    unusablebookmarks.insert(bookmarks_to_check[i]);
                    missing_datafiles.insert(deleteddatfile);
                }
            }
        }

    } while(0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s %s\n",FUNCTION_NAME,deleteddatfile.c_str());
}


bool CDPV3DatabaseImpl::fetch_cdptimestamp(const CDPV3BookMark & bkmk, 
                                           cdp_timestamp_t & cdp_ts)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    try
    {
        do
        {
            SV_TIME svtime;
            if(!CDPUtil::ToSVTime(bkmk.c_timestamp,svtime))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                    FUNCTION_NAME, bkmk.c_timestamp);
                rv = false;
                break;
            }

            cdp_ts.year = svtime.wYear - CDP_BASE_YEAR;
            cdp_ts.month = svtime.wMonth;
            cdp_ts.days = svtime.wDay;
            cdp_ts.hours = svtime.wHour;
            cdp_ts.events = bkmk.c_counter;

        } while (0);
    } 
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, ce.what(), dbdir().c_str());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, e.what(), dbdir().c_str());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception for %s.\n",
            FUNCTION_NAME, dbdir().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV3DatabaseImpl::fetch_cdptimestamp(const CDPV3RecoveryRange & recoverypt, 
                                           cdp_timestamp_t & cdp_ts)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
    try
    {
        do
        {
            SV_TIME svtime;
            if(!CDPUtil::ToSVTime(recoverypt.c_tsfc,svtime))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                    FUNCTION_NAME, recoverypt.c_tsfc);
                rv = false;
                break;
            }

            cdp_ts.year = svtime.wYear - CDP_BASE_YEAR;
            cdp_ts.month = svtime.wMonth;
            cdp_ts.days = svtime.wDay;
            cdp_ts.hours = svtime.wHour;
            cdp_ts.events = 0;

        } while (0);
    } 
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, ce.what(), dbdir().c_str());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, e.what(), dbdir().c_str());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception for %s.\n",
            FUNCTION_NAME, dbdir().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::get_listof_stalefiles(std::list<std::string> &stale_files)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);
	try
	{
		do
		{
			SV_ULONGLONG tsfc, tslc;
			SV_ULONGLONG tsfc_inhrs;
			SV_ULONGLONG filetime;

			getrecoveryrange(tsfc,tslc);

			CDPUtil::ToTimestampinHrs(tsfc,tsfc_inhrs);
			
			// open directory
			ACE_DIR *dirp = sv_opendir(dbdir().c_str());

			if (dirp == NULL)
			{
				int lasterror = ACE_OS::last_error();
				DebugPrintf(SV_LOG_ERROR, 
						"\n%s encountered error %d while reading from %s\n",
						FUNCTION_NAME, lasterror, dbdir().c_str()); 
				rv = false;
				break;
			}

			struct ACE_DIRENT *dp = NULL;
			do
			{
				ACE_OS::last_error(0);
				// get directory entry
				if ((dp = ACE_OS::readdir(dirp)) != NULL)
				{
					// find a matching entry for dat files.
					const std::string mdfiles = CDPV3_MD_PREFIX + "*" + CDPV3_MD_SUFFIX;
					const std::string datafiles = CDPV3_DATA_PREFIX + "*" + CDPV3_DATA_SUFFIX;
					const std::string dellogfiles = CDPV3_COALESCED_PREFIX + "*" + CDPV3_COALESCED_SUFFIX;
					if(RegExpMatch(mdfiles.c_str(), ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
					{
						std::string mdfilename = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
						filetime = cdpv3metadatafile_t::getmetadatafile_timestamp(mdfilename);
						if(filetime && (filetime < tsfc_inhrs))
						{
							stale_files.push_back(mdfilename);
						}

					}
					if(RegExpMatch(dellogfiles.c_str(), ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
					{
						std::string dellogfilename = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
						filetime = cdpv3metadatafile_t::getdellogfile_timestamp(dellogfilename);
						if(filetime && (filetime < tsfc_inhrs))
						{
							stale_files.push_back(dellogfilename);
						}

					}
					else if(RegExpMatch(datafiles.c_str(), ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
					{
						SV_ULONGLONG seqno;
						cdp_timestamp_t start_ts;
						cdp_timestamp_t end_ts;
						SV_TIME sv;
						sv.wYear = sv.wMonth = sv.wDay = sv.wHour = sv.wMinute = sv.wSecond = sv.wMilliseconds = sv.wMicroseconds =sv.wHundrecNanoseconds =0;

						bool temp = false;

						std::string datafilename = ACE_TEXT_ALWAYS_CHAR(dp->d_name);

						parse_filename(datafilename,seqno,start_ts,end_ts,temp);

						sv.wYear = end_ts.year + CDP_BASE_YEAR;
						sv.wMonth = end_ts.month;
						sv.wDay = end_ts.days;
						sv.wHour = end_ts.hours;
						

						if(!CDPUtil::ToFileTime(sv,filetime))
						{
							rv = false;
							DebugPrintf(SV_LOG_ERROR, "\nFUNCTION  %s Unable to get the file time stamp",
								FUNCTION_NAME);
							break;
						}

						if(filetime < tsfc_inhrs)
						{
							stale_files.push_back(datafilename);
						}
					}
				}

			} while(dp);

			// close directory
			ACE_OS::closedir(dirp);

		} while(false);

	} 
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), dbdir().c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), dbdir().c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, dbdir().c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
	return rv;
}

bool CDPV3DatabaseImpl::delete_stalefiles()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERING:%s\n",FUNCTION_NAME);
	std::list<std::string> stale_files;
	std::string deleted_stale_filelog;
	deleted_stale_filelog = dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A + CDPV3_STALE_FILES_DELLOG;

	do
	{
		if(get_listof_stalefiles(stale_files))
		{

			std::stringstream log;
			if(!stale_files.empty())
			{
		        std::string deletion_time = boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time());

				std::list<std::string>::iterator it = stale_files.begin();

				while(it != stale_files.end())
				{
					std::string filename = dbdir();
					filename += ACE_DIRECTORY_SEPARATOR_CHAR_A;
					filename += *it;
					ACE_OS::last_error(0);
					SV_ULONGLONG size = File::GetSizeOnDisk(filename);
					if(ACE_OS::unlink(getLongPathName(filename.c_str()).c_str()))
					{
						rv = false;
						DebugPrintf(SV_LOG_INFO,"%s - Could not delete the file %s - Error %d\n", FUNCTION_NAME, filename.c_str(), ACE_OS::last_error());
					}
					else
					{
						log <<deletion_time<<","
									 <<*it<<","
									 <<size << "\n"; //Check if size is required. This could be expensive?
					}
					it++;
				}
			}

			if(rv) //If deletion of stale files complete, log the entries to a file
			{
				ofstream coalesced_log;
				coalesced_log.open(deleted_stale_filelog.c_str(), ios::app);

				if(!coalesced_log)
				{
					DebugPrintf(SV_LOG_ERROR, "%s - failed to open %s\n", FUNCTION_NAME,deleted_stale_filelog.c_str());
					rv = false;
					break;
				}
				
				coalesced_log << log.str();

				coalesced_log.flush();
				coalesced_log.close();
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"%s:Could not retrieve list of stale files\n",FUNCTION_NAME);
		}
	}while(false);

	DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
	return rv;
}

bool CDPV3DatabaseImpl::delete_unusable_recoverypts()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED:%s\n",FUNCTION_NAME);

    try 
    {
        do
        {
            SV_ULONGLONG recovery_start_ts = 0;
            SV_ULONGLONG recovery_end_ts = 0;
            std::vector<CDPV3RecoveryRange> timeranges_in_db;
            std::vector<CDPV3BookMark> bookmarks_in_db;

            std::set<CDPV3RecoveryRange> unusablepoints; 
            std::set<CDPV3BookMark> unusablebookmarks;
            std::set<std::string> missing_datafiles;


            getrecoveryrange(recovery_start_ts,recovery_end_ts);

            connect();

            if(!fetch_timeranges_in_db(recovery_start_ts,recovery_end_ts,timeranges_in_db))
            {
                rv = false;
                break;
            }

            if(!fetch_bookmarks_in_timerange(recovery_start_ts,recovery_end_ts, bookmarks_in_db))
            {
                rv = false;
                break;
            }

            shutdown();

            detect_unusable_recoverypoints(recovery_start_ts, recovery_end_ts,timeranges_in_db,
                bookmarks_in_db, unusablepoints, unusablebookmarks, missing_datafiles);

			//Log file should be created even if there are no unusablepoints or unusablebookmarks.
			//The operation is executed only once to cleanup any unusable entries due to an issue in the code.
            if(delete_recovery_pts(unusablepoints, unusablebookmarks))
			{
                log_deleted_unusablepoints(unusablepoints, unusablebookmarks);
            }

        }while(0);

    } 
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, ce.what(), dbdir().c_str());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, e.what(), dbdir().c_str());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception for %s.\n",
            FUNCTION_NAME, dbdir().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::delete_recovery_pts(const std::set<CDPV3RecoveryRange> & timeranges_to_delete,
                                            const std::set<CDPV3BookMark> &bookmarks_to_delete)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do
        {
            BeginTransaction();

            std::set<CDPV3BookMark>::const_iterator bkmk_iter = bookmarks_to_delete.begin();
            for( ; bkmk_iter != bookmarks_to_delete.end(); ++bkmk_iter)
            {

                std::stringstream updatequery;
                updatequery << " update " << CDPV3BOOKMARKSTBLNAME
                    << " set c_cxupdatestatus = " 
                    << CS_DELETE_PENDING
                    << " where c_entryno = " 
                    << (*bkmk_iter).c_entryno;

                sqlite3_command update_cmd(*m_con, updatequery.str() );
                update_cmd.executenonquery();
            }


            std::set<CDPV3RecoveryRange>::const_iterator range_iter = timeranges_to_delete.begin();
            for( ; range_iter != timeranges_to_delete.end(); ++range_iter)
            {
                CDPV3RecoveryRange timerange = *range_iter;
                std::stringstream updatequery;
                updatequery << "update " << CDPV3RECOVERYRANGESTBLNAME 
                    << " set c_cxupdatestatus = " 
                    << CS_DELETE_PENDING
                    << " where c_entryno = " 
                    << timerange.c_entryno;

                sqlite3_command update_cmd(*m_con, updatequery.str() );
                update_cmd.executenonquery();
            }

            CommitTransaction();

        } while (0);


        shutdown();


    } catch( std::exception & ex )	{
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while updating CS information to the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


bool CDPV3DatabaseImpl::log_deleted_unusablepoints(const std::set<CDPV3RecoveryRange> & unusablepoints,
                                                   const std::set<CDPV3BookMark> &unusablebookmarks)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERING:%s\n",FUNCTION_NAME);
    std::string unusablepoints_filelog;
    unusablepoints_filelog = dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A + CDPV3_UNUSABLE_RECOVERYPTS_DELLOG;

    do
    {
        ofstream log_strm;
        log_strm.open(unusablepoints_filelog .c_str(), ios::app);

        if(!log_strm)
        {
            DebugPrintf(SV_LOG_ERROR, "%s - failed to open %s\n", FUNCTION_NAME,unusablepoints_filelog.c_str());
            rv = false;
            break;
        }

        std::string deletion_time = boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time());

        std::set<CDPV3BookMark>::const_iterator bkmk_iter = unusablebookmarks.begin();
        for( ; bkmk_iter != unusablebookmarks.end(); ++bkmk_iter)
        {

            log_strm << deletion_time<< "," << (*bkmk_iter) << "\n";
        }

        std::set<CDPV3RecoveryRange>::const_iterator range_iter = unusablepoints.begin();
        for( ; range_iter != unusablepoints.end(); ++range_iter)
        {

            log_strm << deletion_time<< "," << (*range_iter) << "\n";
        }


        log_strm.flush();
        log_strm.close();

    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED:%s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV3DatabaseImpl::calculate_sparsesavings(SV_LONGLONG& spacesavedbysparse)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	const int column_number = 2;
	const std::string pattern = CDPV3_COALESCED_PREFIX + std::string("*") + CDPV3_COALESCED_SUFFIX; 
	std::string delimiter = std::string(",");
	std::string logfilepath;
	spacesavedbysparse = 0;

	svector_t lognames;


	//get the list of log files		
	if(CDPUtil::get_filenames_withpattern(m_dbdir,pattern,lognames))
	{
		svector_t::const_iterator iter = lognames.begin();
		while(iter != lognames.end())
		{
			svector_t deletedfile_sizes;
			logfilepath.clear();
			logfilepath = m_dbdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + *iter;
			if(!CDPUtil::parse_and_get_specifiedcolumn(logfilepath,delimiter,column_number,deletedfile_sizes))
			{
				rv = false;
				break;
			}

			svector_t::iterator sizeiter = deletedfile_sizes.begin();
			while(sizeiter != deletedfile_sizes.end())
			{
				try
				{
					spacesavedbysparse += boost::lexical_cast<SV_LONGLONG>(*sizeiter);
				}
				catch(boost::bad_lexical_cast& ex)
				{	
					DebugPrintf(SV_LOG_ERROR," Exception caught : %s\n",ex.what());
					rv = false;
					break;
				}

				sizeiter++;
			}
			if(!rv ) //if internal loop  fails,dont iterate further
			{
				break;
			}

			iter++;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR," Failed to get log files list from the retention dir :%s.\n",m_dbdir.c_str());
		rv = false;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

void CDPV3DatabaseImpl::fetch_readahead_configvalues()
{
    LocalConfigurator lc;
    m_isreadaheadcache_enabled = lc.isCDPReadAheadCacheEnabled();
    m_readahead_threads = lc.CDPReadAheadThreads();
    m_readahead_file_count = lc.CDPReadAheadFileCount();
    m_readahead_length = lc.CDPReadAheadLength();
}
