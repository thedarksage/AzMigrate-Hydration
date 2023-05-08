//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv1databaseimpl.cpp
//
// Description: 
//

#include "cdpv1databaseimpl.h"
#include "portablehelpers.h"
#include "cdputil.h"
#include "cdplock.h"
#include "file.h"
#include "differentialfile.h"
#include "cdpplatform.h"
#include "inmageex.h"
#include "localconfigurator.h"
#include "configwrapper.h"
#include "StreamRecords.h"

#include <sstream>


using namespace sqlite3x;
using namespace std;

CDPV1DatabaseImpl::CDPV1DatabaseImpl(const std::string & dbname)
: CDPDatabaseImpl(dbname)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, dbname.c_str());

    m_versionAvailableInCache = false;

    m_drtdcndn = 0;
    m_eventinodeno = 0;
    m_inoderead = false;
    m_diffinfoIndex = 0;
    m_partialtxnread = false;
    m_change = 0;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, dbname.c_str());
}

CDPV1DatabaseImpl::~CDPV1DatabaseImpl()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
}

//
// FUNCTION NAME :  initialize
//
// DESCRIPTION : creates and initializes database tables if required
//
//
// INPUT PARAMETERS : None
//                   
//
// OUTPUT PARAMETERS : None
//
//
// return value : true if success, else false
//
//
bool CDPV1DatabaseImpl::initialize()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();
        BeginTransaction();

        std::stringstream create_version_table ;
        std::stringstream create_superblock_table ;
        std::stringstream create_datadirs_table ;
        std::stringstream create_inode_table ;
        std::stringstream create_event_table ;
        std::stringstream create_pendingevent_table ;
        std::stringstream create_timerange_table ;
        std::stringstream create_pendingtimerange_table ;
        std::stringstream create_applied_table ;
        std::vector<std::string> stmts;


        create_version_table 
            << "create table " 
            << " if not exists "
            << CDPV1VERSIONTBLNAME
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_VERSIONTBL_COLS ; ++i )
        {
            if(i)
                create_version_table << ",";
            create_version_table << CDPV1_VERSION_COLS[i];
        }
        create_version_table << ");" ;

        stmts.push_back(create_version_table.str());


        create_superblock_table 
            << "create table " 
            << " if not exists "
            << CDPV1SUPERBLOCKTBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_SUPERBLOCK_COLS ; ++i )
        {
            if(i)
                create_superblock_table << ",";
            create_superblock_table << CDPV1_SUPERBLOCK_COLS[i];
        }
        create_superblock_table << ");" ;

        stmts.push_back(create_superblock_table.str());

        create_datadirs_table 
            << "create table " 
            << " if not exists "
            << CDPV1DATADIRTBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_DATADIR_COLS ; ++i )
        {
            if(i)
                create_datadirs_table << ",";
            create_datadirs_table << CDPV1_DATADIR_COLS[i];
        }
        create_datadirs_table << ");" ;

        stmts.push_back(create_datadirs_table.str());


        create_inode_table 
            << "create table " 
            << " if not exists "
            << CDPV1INODETBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_INODE_COLS ; ++i )
        {
            if(i)
                create_inode_table << ",";
            create_inode_table << CDPV1_INODE_COLS[i];
        }

        create_inode_table << ");" ;

        stmts.push_back(create_inode_table.str());

        create_event_table 
            << "create table "
            << " if not exists "
            << CDPV1EVENTTBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_EVENT_COLS ; ++i )
        {
            if(i)
                create_event_table << ",";
            create_event_table << CDPV1_EVENT_COLS[i];
        }

        create_event_table << ");" ;

        stmts.push_back(create_event_table.str());

        create_pendingevent_table 
            << "create table " 
            << " if not exists "
            << CDPV1PENDINGEVENTTBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_PENDINGEVENT_COLS ; ++i )
        {
            if(i)
                create_pendingevent_table << ",";
            create_pendingevent_table << CDPV1_PENDINGEVENT_COLS[i];
        }

        create_pendingevent_table << ");" ;

        stmts.push_back(create_pendingevent_table.str());

        create_timerange_table 
            << "create table " 
            << " if not exists "
            << CDPV1TIMERANGETBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_TIMERANGE_COLS ; ++i )
        {
            if(i)
                create_timerange_table << ",";
            create_timerange_table << CDPV1_TIMERANGE_COLS[i];
        }

        create_timerange_table << ");" ;

        stmts.push_back(create_timerange_table.str());

        create_pendingtimerange_table 
            << "create table " 
            << " if not exists "
            << CDPV1PENDINGTIMERANGETBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_PENDINGTIMERANGE_COLS ; ++i )
        {
            if(i)
                create_pendingtimerange_table << ",";
            create_pendingtimerange_table << CDPV1_PENDINGTIMERANGE_COLS[i];
        }

        create_pendingtimerange_table << ");" ;

        stmts.push_back(create_pendingtimerange_table.str());

        create_applied_table 
            << "create table " 
            << " if not exists "
            << CDPV1APPLIEDTBLNAME 
            << "(" ;

        for (int i = 0; i < CDPV1_NUM_APPLIED_COLS ; ++i )
        {
            if(i)
                create_applied_table << ",";
            create_applied_table << CDPV1_APPLIED_COLS[i];
        }

        create_applied_table << ");" ;

        stmts.push_back(create_applied_table.str());

        executestmts(stmts);

        stmts.clear();

        std::stringstream initialize_version_table;
        std::stringstream initialize_superblock_table;
        bool needupgrade = false;

        if(isempty(CDPV1VERSIONTBLNAME))
        {		
            initialize_version_table << "insert into " << CDPV1VERSIONTBLNAME 
                << " ( c_version, c_revision )"
                << " values(" 
                << CDPVER << ","
                << CDPVERCURRREV
                << ");" ;

            stmts.push_back(initialize_version_table.str());
        }
        else
        {
            needupgrade = true;
        }

        if(isempty(CDPV1SUPERBLOCKTBLNAME))
        {
            initialize_superblock_table << "insert into " << CDPV1SUPERBLOCKTBLNAME 
                << " ( c_type, c_lookuppagesize, c_physicalspace, " 
                << "   c_tsfc, c_tslc, c_numinodes, c_eventsummary )"
                << " values(" 
                << CDP_UNDO << ","
                << CDP_LOOKUPPAGESIZE << ","
                << 0 << ","
                << 0 << ","
                << 0 << ","
                << 0 << ","
                << "'" << "" << "'"
                << ");" ;

            stmts.push_back(initialize_superblock_table.str());
        }

        executestmts(stmts);
        stmts.clear();

        if(needupgrade && !upgrade())
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to upgrade the db %s\n", FUNCTION_NAME, m_dbname.c_str());
            rv = false;
            shutdown();
        }
        else
        {
            CommitTransaction();
            shutdown();
            rv = true;
        }

    } catch (std::exception & ex)
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while initializing the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    return rv;
}

//
// FUNCTION NAME :  getcdpsummary
//
// DESCRIPTION : Fetches the  summary information such as,
//				start/end timestamp, num of files, space, events
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
bool CDPV1DatabaseImpl::getcdpsummary(CDPSummary & summary)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    try
    {
        connect();

        getcdpsummary_nolock(summary);

        shutdown();
        rv = true;

    } catch( std::exception & ex )	{
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

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  get_cdp_retention_diskusage_summary
//
// DESCRIPTION : Fetches the  summary information such as,
//				start/end timestamp, num of files, size on disk,space saved by compression or sparse
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
bool CDPV1DatabaseImpl::get_cdp_retention_diskusage_summary(CDPRetentionDiskUsage &cdpretentiondiskusage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    try
    {
        connect();

        get_cdp_retention_diskusage_summary_nolock(cdpretentiondiskusage);

        shutdown();
        rv = true;

    } catch( std::exception & ex )	{
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

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1DatabaseImpl::getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = false;

    try
    {
        connect();

        CDPV1SuperBlock superblock;
        ReadSuperBlock(superblock);
        start = superblock.c_tsfc;
        end = superblock.c_tslc;

        shutdown();
        rv = true;

    } catch( std::exception & ex )	{
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

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
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
bool CDPV1DatabaseImpl::fetchseq_with_endtime_greq( SV_ULONGLONG & ts, SV_ULONGLONG & seq )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;
    bool found = false;

    try
    {

        connect();

        do 
        {
            stringstream query ;
            query << "select * from " << CDPV1INODETBLNAME 
                << " where c_endtime >= " 
                <<  ts 
                << " order by c_inodeno ASC limit 1" ;

            sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
            m_cmd.reset(cmd);
            m_reader = m_cmd -> executereader();

            CDPV1Inode inode;
            SVERROR hr_read;

            if(ReadInode(inode,true))
            {                   
                // if this inode does not support per i/o sequence, return zero
                if(inode.c_version < CDPV1_INODE_PERIOVERSION)
                {
                    seq = 0;
                    rv = true;
                    found = true;
                    break;
                }

                if(inode.c_type != NONSPLIT_IO)
                {
                    seq = 0;
                    rv = true;
                    found = true;
                    break;
                }

                for (int diffnum = 0 ;diffnum < inode.c_numdiffs ; ++diffnum)
                {
                    CDPV1DiffInfoPtr diffPtr = inode.c_diffinfos[diffnum];
                    if(diffPtr -> tsfc >= ts)
                    {
                        seq = diffPtr -> tsfcseq;
                        rv = true;
                        found = true;
                        break;
                    }

                    cdp_drtdv2s_iter_t drtdsIter = diffPtr -> changes.begin();
                    cdp_drtdv2s_iter_t drtdsEnd = diffPtr -> changes.end();

                    for( ; drtdsIter != drtdsEnd ; ++drtdsIter )
                    {
                        cdp_drtdv2_t drtd = *drtdsIter;

                        if((diffPtr -> tsfc + drtd.get_timedelta()) >= ts)
                        {
                            seq = (diffPtr -> tsfcseq + drtd.get_seqdelta());
                            rv = true;
                            found = true;
                            break;
                        }
                    }

                    if(found)
                    {
                        break;
                    }

                    if(diffPtr -> tslc >= ts)
                    {
                        seq = diffPtr -> tslcseq;
                        rv = true;
                        found = true;
                        break;
                    }
                }
            }

            if(!found)
            {
                rv = false;
            }

        } while (0);

        shutdown();
    }

    catch (std::exception & ex)
    {
        std::stringstream l_stderr; 
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n" 
            << "Exception Occured during read operation\n" 
            << " Database : " << m_dbname << "\n" 
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ; 

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str()); 
        shutdown();
        rv = false; 
    }

    return rv;
}

bool CDPV1DatabaseImpl::delete_olderdata(const std::string& volname,
                                         SV_ULONGLONG & start_ts,
                                         const SV_ULONGLONG & timeRangeTobeFreed)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do 
        {
            SV_LONGLONG freedTimeRange = 0;
            SV_LONGLONG inode_count = 0;
            CDPLock vol_lock(volname);
            CDPV1SuperBlock superblock;
            ReadSuperBlock(superblock);
            inode_count = superblock.c_numinodes;

            CDPV1Inode inode;
            while (freedTimeRange < timeRangeTobeFreed)
            {
                if(!FetchOldestInode(inode, false))
                    break;


                --inode_count;

                if(inode_count == 0)
                {
					//Current retention file purging is done by dataprotection. This is done to avoid lock contention
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
                }

                if(IsVsnapDriverAvailable())
                {
                    CDPUtil::UnmountVsnapsAssociatedWithInode(inode.c_endtime,volname);
                }

                BeginTransaction();

                ReadSuperBlock(superblock);

                string datafilefullname = inode.c_datadir ;
                datafilefullname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                datafilefullname += inode.c_datafile;

                // PR#10815: Long Path support
                if ( 0 !=  ACE_OS::unlink(getLongPathName(datafilefullname.c_str()).c_str()) )
                {
                    stringstream l_stdfatal;
                    l_stdfatal << "Error detected  " << "in Function:" 
                        << FUNCTION_NAME << " @ Line:" << LINE_NO
                        << " FILE:" << FILE_NAME << "\n\n"
                        << " Error occured on unlink of data file: " 
                        << datafilefullname << "\n"
                        << " Error Code : " << Error::Code() << "\n"
                        << " Error Message:" << Error::Msg() << "\n"
                        << " Please delete the file manually\n";

                    DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());			
                    //rv = false;
                    //break;
                }

                freedTimeRange += (inode.c_endtime - superblock.c_tsfc);

                CDPEvents cdpevents;
                FetchEventsForInode(inode, cdpevents);


                CDPEvents::iterator events_iter = cdpevents.begin();
                CDPEvents::iterator events_end  = cdpevents.end();
                for( ; events_iter != events_end ; ++events_iter)
                {
                    CDPEvent cdpevent = *events_iter;
                    DeleteEvent(cdpevent);
                    superblock.c_eventsummary[cdpevent.c_eventtype] -= 1;
                }

                superblock.c_numinodes -= 1;
                assert(superblock.c_numinodes >= 0);
                if(superblock.c_numinodes < 0)
                    superblock.c_numinodes = 0;

                // scenario:
                // in the apply code path, 
                //   new inode is created with incoming diff start time (see AllocateNewInodeIfRequired routine)
                //   later apply fails when trying to write to the data file
                // superblock contains: 2011-07-01 10:30 to 2011-07-01 10:35
                // newest entry in inode table contains: 2011-07-01 10:37
                // now on insufficient space we purge all the datafiles (inodes).
                // superblock tsfc is updated above with value: 2011-07-01 10:37
                // so superblock table has value 2011-07-01 10:37 to 2011-07-01 10:35
                //
                if(superblock.c_tslc < inode.c_endtime)
                {
                    superblock.c_tsfc = superblock.c_tslc;
                } else
                {
                    superblock.c_tsfc = inode.c_endtime;
                }

                if((superblock.c_tsfc == superblock.c_tslc) && (0 == superblock.c_numinodes))
                {
                    superblock.c_tsfc = superblock.c_tslc = 0;
                }

                if(inode.c_physicalsize <= superblock.c_physicalspace)
                {
                    superblock.c_physicalspace -= inode.c_physicalsize;
                }else
                {
                    DebugPrintf(SV_LOG_ERROR, "inode.c_physicalsize:" ULLSPEC
                        " greater than superblock.c_physicalspace:" ULLSPEC "\n",
                        inode.c_physicalsize, superblock.c_physicalspace);
                    superblock.c_physicalspace = 0;
                }

                DeleteInode(inode);
                UpdateSuperBlock(superblock);
                AdjustTimeRangeTable(superblock);

                CommitTransaction();
            }

            start_ts =  superblock.c_tsfc;

        } while (0);


        shutdown();
    }
    catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while enforcing policies. database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1DatabaseImpl::purge_retention(const std::string& volname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    try
    {
        connect();

        do 
        {
            SV_LONGLONG inode_count = 0;
            CDPV1SuperBlock superblock;
            CDPV1Inode inode;

            ReadSuperBlock(superblock);
            inode_count = superblock.c_numinodes;


			while(FetchOldestInode(inode,false))
			{

				if(IsVsnapDriverAvailable())
				{
					CDPUtil::UnmountVsnapsAssociatedWithInode(inode.c_endtime,volname);
				}


				BeginTransaction();

				ReadSuperBlock(superblock);

				string datafilefullname = inode.c_datadir ;
				datafilefullname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
				datafilefullname += inode.c_datafile;

				// PR#10815: Long Path support
				if ( 0 !=  ACE_OS::unlink(getLongPathName(datafilefullname.c_str()).c_str()) )
				{
					stringstream l_stdfatal;
					l_stdfatal << "Error detected  " << "in Function:" 
						<< FUNCTION_NAME << " @ Line:" << LINE_NO
						<< " FILE:" << FILE_NAME << "\n\n"
						<< " Error occured on unlink of data file: " 
						<< datafilefullname << "\n"
						<< " Error Code : " << Error::Code() << "\n"
						<< " Error Message:" << Error::Msg() << "\n"
						<< " Please delete the file manually\n";

					DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());			
					//rv = false;
					//break;
				}


				CDPEvents cdpevents;
				FetchEventsForInode(inode, cdpevents);


				CDPEvents::iterator events_iter = cdpevents.begin();
				CDPEvents::iterator events_end  = cdpevents.end();
				for( ; events_iter != events_end ; ++events_iter)
				{
					CDPEvent cdpevent = *events_iter;
					DeleteEvent(cdpevent);
					superblock.c_eventsummary[cdpevent.c_eventtype] -= 1;
				}

				superblock.c_numinodes -= 1;
				assert(superblock.c_numinodes >= 0);
				if(superblock.c_numinodes < 0)
					superblock.c_numinodes = 0;

				// scenario:
				// in the apply code path, 
				//   new inode is created with incoming diff start time (see AllocateNewInodeIfRequired routine)
				//   later apply fails when trying to write to the data file
				// superblock contains: 2011-07-01 10:30 to 2011-07-01 10:35
				// newest entry in inode table contains: 2011-07-01 10:37
				// now on insufficient space we purge all the datafiles (inodes).
				// superblock tsfc is updated above with value: 2011-07-01 10:37
				// so superblock table has value 2011-07-01 10:37 to 2011-07-01 10:35
				//
				if(superblock.c_tslc < inode.c_endtime)
				{
					superblock.c_tsfc = superblock.c_tslc;
				} else
				{
					superblock.c_tsfc = inode.c_endtime;
				}

				if((superblock.c_tsfc == superblock.c_tslc) && (0 == superblock.c_numinodes))
				{
					superblock.c_tsfc = superblock.c_tslc = 0;
				}

				if(inode.c_physicalsize <= superblock.c_physicalspace)
				{
					superblock.c_physicalspace -= inode.c_physicalsize;
				}else
				{
					DebugPrintf(SV_LOG_ERROR, "inode.c_physicalsize:" ULLSPEC
						" greater than superblock.c_physicalspace:" ULLSPEC "\n",
						inode.c_physicalsize, superblock.c_physicalspace);
					superblock.c_physicalspace = 0;
				}

				DeleteInode(inode);
				UpdateSuperBlock(superblock);
				AdjustTimeRangeTable(superblock);

				CommitTransaction();
			}

        } while (0);


        shutdown();
    }
    catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while enforcing policies. database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1DatabaseImpl::enforce_policies(const CDP_SETTINGS & settings, 
                                         SV_ULONGLONG & start_ts, SV_ULONGLONG & end_ts,const std::string& volname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    SV_LONGLONG SpaceTobeFreed = 0;
    SV_LONGLONG timeRangeTobeFreed = 0;

    SV_LONGLONG freedSpace = 0;
    SV_LONGLONG freedTimeRange = 0;
    SV_LONGLONG freedReservedSpace = 0;
    bool timeBasedPrunningDone = false;
    bool spaceBasedPrunningDone = false;

    try
    {
        connect();


        do 
        {
            SV_ULONGLONG diskpolicy = settings.DiskSpace();
            SV_ULONGLONG timepolicy = settings.TimePolicy();

            SV_LONGLONG inode_count;
            CDPLock vol_lock(volname);
            CDPV1SuperBlock superblock;
            ReadSuperBlock(superblock);
            end_ts = superblock.c_tslc;
            start_ts = superblock.c_tsfc;
            inode_count = superblock.c_numinodes;

            SV_ULONGLONG usedSpace = 0;
            SV_ULONGLONG recoveryrange = superblock.c_tslc - superblock.c_tsfc;

            if(diskpolicy != 0)
            {
                usedSpace = superblock.c_physicalspace + File::GetSizeOnDisk(m_dbname);
                if(usedSpace > diskpolicy)
                {
                    SpaceTobeFreed = usedSpace - diskpolicy;
                }else
                {
                    spaceBasedPrunningDone = true;
                }

            }else
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

            CDPV1Inode inode;
            while (FetchOldestInode(inode, false))
            {
                // acquire lock when deleting last file
                // this is to prevent from simultaneous to same file from apply thread
                // and purge thread
                --inode_count;
                if(inode_count== 0)
                {
                    //we will keep the latest data file
                    //if retention space is insufficient 
                    //then it will be deleted in retention purging due to insufficient space
                    rv = true;
                    break;
                }

                if(IsVsnapDriverAvailable())
                {
                    CDPUtil::UnmountVsnapsAssociatedWithInode(inode.c_endtime,volname);
                }

                BeginTransaction();
                ReadSuperBlock(superblock);

                freedTimeRange += (inode.c_endtime - superblock.c_tsfc);

                if(freedTimeRange >= timeRangeTobeFreed)
                {
                    timeBasedPrunningDone = true;
                }
                if(freedSpace >= SpaceTobeFreed)
                {
                    spaceBasedPrunningDone = true;
                }

                if((spaceBasedPrunningDone && timeBasedPrunningDone))
                {
                    rv = true;
                    break;
                }



                string datafilefullname = inode.c_datadir ;
                datafilefullname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                datafilefullname += inode.c_datafile;

                // PR#10815: Long Path support
                if ( 0 !=  ACE_OS::unlink(getLongPathName(datafilefullname.c_str()).c_str()) )
                {
                    stringstream l_stdfatal;
                    l_stdfatal << "Error detected  " << "in Function:" 
                        << FUNCTION_NAME << " @ Line:" << LINE_NO
                        << " FILE:" << FILE_NAME << "\n\n"
                        << " Error occured on unlink of data file: " 
                        << datafilefullname << "\n"
                        << " Error Code : " << Error::Code() << "\n"
                        << " Error Message:" << Error::Msg() << "\n"
                        << " Please delete the file manually\n";

                    DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());			
                    //rv = false;
                    //break;
                }

                DeleteInode(inode);
                CDPEvents cdpevents;
                FetchEventsForInode(inode, cdpevents);

                CDPEvents::iterator events_iter = cdpevents.begin();
                CDPEvents::iterator events_end  = cdpevents.end();
                for( ; events_iter != events_end ; ++events_iter)
                {
                    CDPEvent cdpevent = *events_iter;
                    DeleteEvent(cdpevent);

                    superblock.c_eventsummary[cdpevent.c_eventtype] -= 1;
                }

                if ( events_iter != events_end )
                {
                    rv = false;
                    break;
                }


                superblock.c_numinodes -= 1;
                assert(superblock.c_numinodes >= 0);
                if(superblock.c_numinodes < 0)
                    superblock.c_numinodes = 0;

                // scenario:
                // in the apply code path, 
                //   new inode is created with incoming diff start time (see AllocateNewInodeIfRequired routine)
                //   later apply fails when trying to write to the data file
                // superblock contains: 2011-07-01 10:30 to 2011-07-01 10:35
                // newest entry in inode table contains: 2011-07-01 10:37
                // now on insufficient space we purge all the datafiles (inodes).
                // superblock tsfc is updated above with value: 2011-07-01 10:37
                // so superblock table has value 2011-07-01 10:37 to 2011-07-01 10:35
                //
                if(superblock.c_tslc < inode.c_endtime)
                {
                    superblock.c_tsfc = superblock.c_tslc;
                } else
                {
                    superblock.c_tsfc = inode.c_endtime;
                }

                if((superblock.c_tsfc == superblock.c_tslc) && (0 == superblock.c_numinodes))
                {
                    superblock.c_tsfc = superblock.c_tslc = 0;
                }

                if(inode.c_physicalsize <= superblock.c_physicalspace)
                {
                    superblock.c_physicalspace -= inode.c_physicalsize;
                }else
                {
                    DebugPrintf(SV_LOG_ERROR, "inode.c_physicalsize:" ULLSPEC
                        " greater than superblock.c_physicalspace:" ULLSPEC "\n",
                        inode.c_physicalsize, superblock.c_physicalspace);
                    superblock.c_physicalspace = 0;
                }

                if(!spaceBasedPrunningDone)
                {
                    freedSpace += inode.c_physicalsize;
                }

                UpdateSuperBlock(superblock);
                AdjustTimeRangeTable(superblock);
                CommitTransaction();

                end_ts = superblock.c_tslc;
                start_ts = superblock.c_tsfc;
            }

        } while (0);


        shutdown();
    }
    catch( std::exception & ex )
    {
        std::stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while enforcing policies. database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        shutdown();
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


//
// FUNCTION NAME :  setoperation
//
// DESCRIPTION : Prepares the database for further retrieval of event/timerange/drtd etc
//
// INPUT PARAMETERS : 1. QUERYOP - CDPEVENT, CDPDRTD, CDPTIMEANGE etc
//                    2. associated condition e.g CDPMarkersMatchingCndn, CDPTimerangeMatchingCndn etc
//
// OUTPUT PARAMETERS : None
//
//
// return value : 
//
//
void CDPV1DatabaseImpl::setoperation(QUERYOP op, void const * context)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_op = op;
    m_context = context;

    std::string query;

    if(m_op == CDPEVENT)
    {
        CDPMarkersMatchingCndn const * event_cndn = 
            reinterpret_cast<CDPMarkersMatchingCndn const *>(m_context);
        form_event_query(*event_cndn, query);
    } 
    else if(m_op == CDPTIMERANGE)
    {
        CDPTimeRangeMatchingCndn const * timerange_cndn = 
            reinterpret_cast<CDPTimeRangeMatchingCndn const *>(m_context);
        form_timerange_query(*timerange_cndn, query);
    }
    else if (m_op == CDPDRTD)
    {
        m_drtdcndn = reinterpret_cast<CDPDRTDsMatchingCndn const *>(m_context);
        std::stringstream querystream ;
        querystream << "select * from " 
            << CDPV1INODETBLNAME;
        if(m_drtdcndn->fromTime())
        {
            querystream << " where c_starttime <= " << m_drtdcndn->fromTime();
        }
        querystream << " order by c_inodeno DESC" ;
        query = querystream.str();
    }
    else if (m_op == CDPAPPLIEDDRTD)
    {
        std::stringstream querystream ;
        querystream << "select * from " << CDPV1INODETBLNAME << " order by c_inodeno DESC limit 1";
        query = querystream.str();
    } else
    {
        throw INMAGE_EX("unsupported operation.\n");
    }

    connect();
    sqlite3_command * cmd = new sqlite3_command(*(m_con),query);
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

//
// FUNCTION NAME :  fetchVerifiedEvent
//
// DESCRIPTION : prepare the query, fetch the events from event table and prepare the list of pendingenents
//
// INPUT PARAMETERS : CDPMarkersMatchingCndn
//
// OUTPUT PARAMETERS : list of CDPV1PendingEvent
//
//
// return value : true on success otherwise false
//
//
bool CDPV1DatabaseImpl::fetchVerifiedEvent(const CDPMarkersMatchingCndn & cndn,std::vector<CDPV1PendingEvent> & pendingEventList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    std::string query;
    try
    {
        connect();
        form_event_query(cndn, query);
        sqlite3_command * cmd = new sqlite3_command(*(m_con),query);
        m_cmd.reset(cmd);
        m_reader = m_cmd -> executereader();

        CDPEvent cdpEvent;
        while(ReadEvent(cdpEvent))
        {
            CDPV1PendingEvent pendingevent;
            pendingevent.c_eventtype  = cdpEvent.c_eventtype;
            pendingevent.c_eventtime  = cdpEvent.c_eventtime;
            pendingevent.c_eventvalue = cdpEvent.c_eventvalue;
            pendingevent.c_valid      = true;
            pendingevent.c_comment = cndn.comment();
            pendingevent.c_identifier = cdpEvent.c_identifier;
            pendingevent.c_verified = VERIFIEDEVENT;
            pendingEventList.push_back(pendingevent);
        }

        m_reader.close();
    }catch (exception &ex)	{
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while updating database :" 
            << " Database : " << m_dbname << "\n"
            << " Table Name : " << CDPV1EVENTTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }
    shutdown();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;

}

//
// FUNCTION NAME :  update_verified_events
//
// DESCRIPTION : 1. insert a row for updation to cx in pendingevent table (mark the event as verified)
//				 2. update the event table  (mark the event as verified)               
//
// INPUT PARAMETERS : CDPMarkersMatchingCndn
//
// OUTPUT PARAMETERS : list of CDPV1PendingEvent
//
//
// return value : true on success otherwise false
//
//
bool CDPV1DatabaseImpl::update_verified_events(const CDPMarkersMatchingCndn & cndn)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    std::string query;
    std::vector<CDPV1PendingEvent> pendingEventList;
    do
    {
        try
        {
            if(!fetchVerifiedEvent(cndn,pendingEventList))
            {
                rv = false;
                break;
            }
            if(pendingEventList.size() == 0)
            {
                DebugPrintf(SV_LOG_INFO, "There are no matching bookmarks for the specified input.\n");
                rv = false;
                break;
            }
            std::vector<std::string> stmts;
            FormUpdateVerifyEventQuery(cndn, query);
            stmts.push_back(query);
            std::vector<CDPV1PendingEvent>::iterator iter = pendingEventList.begin();
            while(iter != pendingEventList.end())
            {
                stringstream  action;
                action << "insert into " << CDPV1PENDINGEVENTTBLNAME
                    << " ( c_inodeno,c_eventtype,c_eventtime,c_eventvalue,c_valid ,c_eventmode,c_identifier,c_verified,c_comment)"
                    << " values("
                    << (*iter).c_inodeno     << ","
                    << (*iter).c_eventtype   << ","
                    << (*iter).c_eventtime   << ","
                    << "\"" << (*iter).c_eventvalue << "\"" << ","
                    << (*iter).c_valid << ","
                    << (*iter).c_eventmode << ","
                    << "\"" << (*iter).c_identifier << "\"" << ","
                    << (*iter).c_verified << ","
                    << "\"" << (*iter).c_comment << "\"" << " );" ;
                stmts.push_back(action.str());
                iter++;
            }

            connect();
            BeginTransaction();
            executestmts(stmts);
            CommitTransaction();

        }catch (exception &ex)	{
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
                << "Exception Occured while updating database :" 
                << " Database : " << m_dbname << "\n"
                << " Table Name : " << CDPV1EVENTTBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
        }
        shutdown();
    } while(false);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1DatabaseImpl::update_locked_bookmarks(const CDPMarkersMatchingCndn & cndn,std::vector<CDPEvent> &cdpEvents,SV_USHORT &state)//
{
    DebugPrintf(SV_LOG_INFO, "This operation is not applicable for the specified replication pair.\n");
    return true;
}
// FUNCTION NAME :  read
//
// DESCRIPTION : Reads the next available event from the database
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : CDPEvent
//
// ALGORITHM : a. Creates the event query
//             b. retrieves the event from the database
//
// return value : SVERROR - SVS_OK - if success (and more events are present)
//							SVS_FALSE - if success ( and no further events are present)
//							SVE_FAIL - failure in reading events
//
SVERROR CDPV1DatabaseImpl::read(CDPEvent & event)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVS_OK;

    try
    {
        do 
        {

            if(!good())
            {
                hr = SVE_FAIL;
                break;
            }

            // all events matching condition are provided
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                break;
            }

            event.c_eventno = m_reader.getint64(EVENT_C_EVENTNO_COL);
            event.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_EVENTTYPE_COL));
            event.c_eventtime = m_reader.getint64(EVENT_C_EVENTTIME_COL);
            event.c_eventvalue = m_reader.getblob(EVENT_C_EVENTVALUE_COL); 
            event.c_eventmode = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_EVENTMODE_COL));
            CDPVersion ver;
            GetRevision(ver);
            if(ver.c_revision >= CDPVER1REV3)
            {
                event.c_identifier = m_reader.getblob(EVENT_C_IDENTIFIER_COL);
                event.c_verified = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_VERIFIED_COL));
                event.c_comment = m_reader.getblob(EVENT_C_COMMENT_COL);
                if(event.c_verified == VERIFIEDEVENT)
                {
                    event.c_status = CDP_BKMK_UPDATE_REQUEST;
                } else
                {
                    event.c_status = CDP_BKMK_INSERT_REQUEST;
                }
            }else
            {
                event.c_identifier = "";
                event.c_verified = NONVERIFIEDEVENT;
                event.c_comment = "";
                event.c_status = CDP_BKMK_INSERT_REQUEST;
            }
            event.c_lockstatus = BOOKMARK_STATE_UNLOCKED;
            event.c_lifetime = INM_BOOKMARK_LIFETIME_NOTSPECIFIED;

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
            << "Exception Occured while reading event\n"
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1EVENTTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*(m_con)).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

        shutdown();
        good_status = false;
        hr = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

//
// FUNCTION NAME :  read
//
// DESCRIPTION : Reads the next available timerange from the database
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : CDPTimeRange
//
// ALGORITHM : a. Creates the timerange query
//             b. retrieves the timerange from the database
//
// return value : SVERROR - SVS_OK - if success (and more timeranges are present)
//							SVS_FALSE - if success ( and no further timeranges are present)
//							SVE_FAIL - failure in reading timeranges
//
SVERROR CDPV1DatabaseImpl::read(CDPTimeRange & TimeRange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVS_OK;

    try
    {
        do 
        {
            if(!good())
            {
                hr = SVE_FAIL;
                break;
            }

            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                break;
            }

            TimeRange.c_entryno = m_reader.getint64(TIMERANGE_C_ENTRYNO_COL);
            TimeRange.c_starttime = m_reader.getint64(TIMERANGE_C_STARTTIME_COL);
            TimeRange.c_mode = static_cast<SV_USHORT> (m_reader.getint(TIMERANGE_C_MODE_COL));
            TimeRange.c_endtime = m_reader.getint64(TIMERANGE_C_ENDTIME_COL);
            TimeRange.c_startseq =  m_reader.getint64(TIMERANGE_C_STARTSEQ_COL);
            TimeRange.c_endseq =  m_reader.getint64(TIMERANGE_C_ENDSEQ_COL);
            TimeRange.c_granularity = CDP_ANY_POINT_IN_TIME;
            TimeRange.c_action = CDP_TIMERANGE_INSERT_REQUEST;
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
            << "Exception Occured while reading event\n"
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1TIMERANGETBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*(m_con)).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        shutdown();
        good_status = false;
        hr = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

//
// FUNCTION NAME :  read
//
// DESCRIPTION : Reads the next available DRTD from the database
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : cdp_rollback_drtdptr_t
//
// ALGORITHM : a. If the operation set is CDPDRTD, reads drtds from the db until the CDPDRTDMatchingCondition is satisfied
//                    (event is reached or recoveryPoint/sequenceId is crossed)
//             b. If the operation set is CDPAPPLIEDTDRTD, reads using ReadAppliedDRTD

//
// return value : SVERROR - SVS_OK - if success (and more DRTDs are present)
//							SVS_FALSE - if success ( and no further DRTDs are present)
//							SVE_FAIL - failure in reading DRTDs
//
SVERROR CDPV1DatabaseImpl::read(cdp_rollback_drtd_t & rollback_drtd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    SVERROR hr = SVE_FAIL;

    if(m_op == CDPAPPLIEDDRTD)
    {
        hr = ReadAppliedDRTD(rollback_drtd);
    }

    if(m_op == CDPDRTD)
    {
        hr = ReadDRTD(rollback_drtd);
        if(hr != SVS_OK)
        {
            shutdown();
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    return hr;
}

//
// FUNCTION NAME :  getcdpsummary_nolock
//
// DESCRIPTION : Fetches the  summary information such as,
//				start/end timestamp, num of files, space, events
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : CDPSummary
//
// ALGORITHM : 
//			   a. reads the superblock
//             c. populates the summary info.
//
// return value : true if success, else false
//
//
void CDPV1DatabaseImpl::getcdpsummary_nolock(CDPSummary & summary)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());


    CDPV1SuperBlock superblock;
    ReadSuperBlock(superblock);

    summary.start_ts = superblock.c_tsfc;
    summary.end_ts = superblock.c_tslc;
    summary.event_summary = superblock.c_eventsummary;

    CDPVersion ver;
    GetRevision(ver);
    summary.version = ver.c_version;
    summary.revision = ver.c_revision;
    summary.log_type = superblock.c_type;

    GetDiskFreeCapacity(m_dbdir,summary.free_space);


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
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
// ALGORITHM : 
//			   a. reads the superblock
//             c. populates the CDPRetentionDiskUsage info.
//
//
//
void CDPV1DatabaseImpl::get_cdp_retention_diskusage_summary_nolock(CDPRetentionDiskUsage & retentionusage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());

    CDPV1SuperBlock superblock;
    ReadSuperBlock(superblock);
    SV_ULONGLONG logicalsize = 0;

    retentionusage.start_ts = superblock.c_tsfc;
    retentionusage.end_ts = superblock.c_tslc;
    retentionusage.num_files = superblock.c_numinodes; 
    retentionusage.size_on_disk = superblock.c_physicalspace + File::GetSizeOnDisk(m_dbname);
    retentionusage.space_saved_by_sparsepolicy = CDPV1_SPARSE_SAVINGS_NOTAPPLICABLE;

    SV_ULONGLONG file_count = retentionusage.num_files;
    SV_ULONGLONG sizeondisk = 0;
    std::string pattern = CDPDATAFILEPREFIX + std::string("*");
    if(!sv_get_filecount_spaceusage(m_dbdir.c_str(),pattern.c_str(),file_count,sizeondisk,logicalsize))
    {
        throw INMAGE_EX("unable to get space usage for")(m_dbdir);
    }

    if(logicalsize > retentionusage.size_on_disk)
        retentionusage.space_saved_by_compression = logicalsize - sizeondisk;
    else
        retentionusage.space_saved_by_compression = 0;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}


//
// FUNCTION NAME :  ReadSuperBlock
//
// DESCRIPTION : Reads the superblock from the database
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : CDPV1SuperBlock
//
//
// return value : true if success, else false
//
//
void CDPV1DatabaseImpl::ReadSuperBlock(CDPV1SuperBlock & superblock)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    sqlite3_reader reader;
    std::stringstream query ;
    query << "select * from " << CDPV1SUPERBLOCKTBLNAME;

    boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));
    reader = cmd -> executereader();

    if(!reader.read())
    {
        reader.close();
        throw INMAGE_EX("superblock table is not yet initialized");
        return;
    }

    superblock.c_type = static_cast<CDP_LOGTYPE> (reader.getint(SUPERBLOCK_C_TYPE_COL));
    superblock.c_lookuppagesize = reader.getint64(SUPERBLOCK_C_LOOKUPPAGESIZE_COL);
    superblock.c_physicalspace = reader.getint64(SUPERBLOCK_C_PHYSICALSPACE_COL);
    superblock.c_tsfc = reader.getint64(SUPERBLOCK_C_TSFC_COL);
    superblock.c_tslc = reader.getint64(SUPERBLOCK_C_TSLC_COL);
    superblock.c_numinodes = reader.getint64(SUPERBLOCK_C_NUMINODES_COL);

    SV_EVENT_TYPE EventType;
    SV_ULONGLONG NumEvents;

    superblock.c_eventsummary.clear();
    std::string eventsummarystring = reader.getblob(SUPERBLOCK_C_EVENTSUMMARY_COL);
    svector_t eventinfos = CDPUtil::split(eventsummarystring, ";" );
    for ( unsigned int eventnum =0; eventnum < eventinfos.size() ; ++eventnum)
    {
        std::string tmp = eventinfos[eventnum];
        sscanf(tmp.c_str(), "%hu," LLSPEC, &EventType, &NumEvents);
        superblock.c_eventsummary[EventType] = NumEvents;
    }

    reader.close();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

//
// FUNCTION NAME :  GetRevision
//
// DESCRIPTION : Reads the database revision from version table
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : double - database revision
//
//
// return value : true if sucess, else false
//
//
void CDPV1DatabaseImpl::GetRevision(CDPVersion & ver)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {

        if(m_versionAvailableInCache)
        {
            ver.c_version = m_ver.c_version;
            ver.c_revision = m_ver.c_revision;
            break;
        }

        stringstream query ;
        query << "select * from " << CDPV1VERSIONTBLNAME  << " ; ";

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

        ver.c_version  = reader.getdouble(VERSIONTBL_C_VERSION_COL);
        ver.c_revision = reader.getdouble(VERSIONTBL_C_REVISION_COL);
        m_ver.c_version = ver.c_version;
        m_ver.c_revision = ver.c_revision;
        m_versionAvailableInCache = true;

        reader.close();

    } while ( 0 );

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

//
// FUNCTION NAME :  ReadInode
//
// DESCRIPTION : Reads the next available inode from the database
//
// INPUT PARAMETERS : ReadDiff - true(if the differentials are to be read)
//
// OUTPUT PARAMETERS : CDPV1Inode
//
//
// return value : SVERROR - SVS_OK - if success (and more inodes are present)
//							SVS_FALSE - if success ( and no further inodes are present)
//							SVE_FAIL - failure in reading inode 

bool CDPV1DatabaseImpl::ReadInode(CDPV1Inode & inode, bool ReadDiff)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        try
        {
            if(!m_reader.read())
            {
                rv = false;
                break;
            }

            inode.c_diffinfos.clear();

            inode.c_inodeno = m_reader.getint64(INODE_C_INODENO_COL);
            inode.c_type  = m_reader.getint(INODE_C_TYPE_COL);
            inode.c_dirnum  = m_reader.getint(INODE_C_DIRNUM_COL);
            inode.c_datadir = m_dbdir;
            inode.c_datafile = m_reader.getblob(INODE_C_DATAFILE_COL);
            inode.c_startseq = m_reader.getint(INODE_C_STARTSEQ_COL);
            inode.c_starttime = m_reader.getint64(INODE_C_STARTTIME_COL);
            inode.c_endseq = m_reader.getint(INODE_C_ENDSEQ_COL);
            inode.c_endtime = m_reader.getint64(INODE_C_ENDTIME_COL);
            inode.c_physicalsize = m_reader.getint64(INODE_C_PHYSICALSIZE_COL);
            inode.c_logicalsize = m_reader.getint64(INODE_C_LOGICALSIZE_COL);
            inode.c_numdiffs = m_reader.getint(INODE_C_NUMDIFFS_COL);
            inode.c_numevents = m_reader.getint(INODE_C_NUMEVENTS_COL);

            //
            // the following code block is to allow
            // reading inode operation from older database revision
            // in version 2.0: c_version column was added.
            // the database will anyway get upgraded to latest revision
            // on first differential/resync data file write operation 
            // or prune operation.
            // the only place where we may need to read from older revision
            // would be showsummary,listevent,snapshot immediately after agent
            // upgrade
            CDPVersion ver;           
            GetRevision(ver);
            if(ver.c_revision >= CDPVER1REV2)
            {
                inode.c_version = m_reader.getint(INODE_C_VERSION_COL);
            }
            else
            {
                inode.c_version = CDPV1_INODE_VERSION0; 
            }

            if( (inode.c_version != CDPV1_INODE_VERSION0) 
                && (inode.c_version != CDPV1_INODE_VERSION1))
            {
                stringstream l_stderr;
                l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
                    << "Database: " << m_dbname 
                    << " is corrupted. Differentials will not be applied.\n"
                    << "Error Occured while reading inode information "
                    << "invalid inode version\n"
                    << "inodeno: " << inode.c_inodeno << "\n"
                    << "version" << inode.c_version << "\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }

            if(ReadDiff)
            {
                string diffinfostring = m_reader.getblob(INODE_C_DIFFINFOS_COL);
                svector_t diffinfos = CDPUtil::split(diffinfostring, ";" );
                // ensure diffinfs.size() ==inode.c_numdiffs
                if ( diffinfos.size() != inode.c_numdiffs )
                {
                    stringstream l_stderr;
                    l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                        << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
                        << "Database: "<< m_dbname 
                        << " is corrupted. Differentials will not be applied.\n"
                        << "Error Occured while reading inode information "
                        << "(diffinfos.size != inode.c_numdiffs )\n"
                        << "diffinfos.size is " << diffinfos.size() << "\n"
                        << "inode.c_numdiffs is " << inode.c_numdiffs << "\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    rv = false;
                    break;
                }

                for ( unsigned int diffnum =0; diffnum < diffinfos.size() ; ++diffnum)
                {
                    CDPV1DiffInfoPtr info(new CDPV1DiffInfo);

                    string tmp = diffinfos[diffnum];
                    ptrdiff_t commas = count(tmp.begin(), find(tmp.begin(), tmp.end(), '-'), ',');

                    // ensure 7 commas before encountering "-"
                    if ( commas != 7 )
                    {
                        stringstream l_stderr;
                        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
                            << "Database: "<< m_dbname 
                            << " is corrupted. Differentials will not be applied.\n"
                            << "Improper String format. Cannot read from string:"
                            << tmp <<"\n";
                        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                        rv = false;
                        break;
                    }


                    sscanf(tmp.c_str(), 
                        LLSPEC "," LLSPEC "," ULLSPEC "," ULLSPEC "," ULLSPEC "," ULLSPEC ",%u,%lu",
                        &(info -> start), &(info -> end), 
                        &(info -> tsfcseq), &(info -> tsfc), 
                        &(info -> tslcseq), &(info -> tslc), 
                        &(info -> ctid), &(info -> numchanges));


                    svector_t changeinfos = CDPUtil::split(tmp, "-" );
                    // ensure changeinfos.size == info -> numchanges +1 
                    if ( changeinfos.size() != info -> numchanges + 1 )
                    {
                        stringstream l_stderr;
                        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
                            << "Database: " << m_dbname 
                            << " is corrupted. Differentials will not be applied.\n"
                            << "Error Occured while reading inode information" 
                            << "(changeinfos.size() != info -> numchanges + 1 )\n"
                            << "changeinfos.size is "<< changeinfos.size()
                            << " info -> numchanges is "<< info -> numchanges  << "\n";

                        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                        rv = false;
                        break;
                    }

                    for (unsigned int j = 0; j < info -> numchanges ; ++j)
                    {
                        SV_UINT         length;
                        SV_OFFSET_TYPE  voloffset;
                        SV_OFFSET_TYPE  fileoffset;
                        SV_UINT      	seqdelta;
                        SV_UINT      	timedelta;

                        string change_tmp = changeinfos[j+1];
                        commas = count(change_tmp.begin(), change_tmp.end(), ',');


                        if (inode.c_version == CDPV1_INODE_VERSION0)
                        {

                            if ( commas != 2 )
                            {
                                stringstream l_stderr;
                                l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
                                    << "Database: "<< m_dbname 
                                    << " is corrupted. Differentials will not be applied.\n"
                                    << "Improper String format. Cannot read from string:"
                                    << tmp<< "\n";

                                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                                rv = false;
                                break;
                            }

                            sscanf(change_tmp.c_str(), LLSPEC ",%d," LLSPEC,
                                &(voloffset), &(length), &(fileoffset) );

                            seqdelta = 0;
                            timedelta = 0;
                        } else if (inode.c_version >= CDPV1_INODE_VERSION1)
                        {
                            if ( commas != 4 )
                            {
                                stringstream l_stderr;
                                l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
                                    << "Database: "<< m_dbname 
                                    << " is corrupted. Differentials will not be applied.\n"
                                    << "Improper String format. Cannot read from string:"
                                    << tmp<< "\n";

                                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                                rv = false;
                                break;
                            }

                            sscanf(change_tmp.c_str(), LLSPEC ",%d," LLSPEC ",%d,%d", 
                                &(voloffset), 
                                &(length), 
                                &(fileoffset),
                                &(seqdelta),
                                &(timedelta));

                        }

                        cdp_drtdv2_t change(length,voloffset,fileoffset,seqdelta,timedelta,0);
                        info -> changes.push_back(change);		
                    }

                    if(!rv)
                        break;

                    inode.c_diffinfos.push_back(info);
                }
            }
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "Exception Occured while reading inode information\n"
                << " Database : " << m_dbname << "\n"
                << " Table : " << CDPV1INODETBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

//
// FUNCTION NAME :  FormUpdateVerifyEventQuery
//
// DESCRIPTION : prepare the update query to update the event table to mark the event as verified              
//
// INPUT PARAMETERS : CDPMarkersMatchingCndn
//
// OUTPUT PARAMETERS : list of CDPV1PendingEvent
//
//
// return value : true on success otherwise false
//
//
void CDPV1DatabaseImpl::FormUpdateVerifyEventQuery(const CDPMarkersMatchingCndn & cndn, std::string & querystring)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::vector<std::string> clause;
    if(cndn.type())
    {
        std::stringstream tmp;
        tmp << "c_eventtype =" << cndn.type();
        clause.push_back(tmp.str());
    }

    if(!cndn.value().empty())
    {
        std::stringstream tmp;
        tmp << "c_eventvalue =" << "\"" << cndn.value() << "\"" ;
        clause.push_back(tmp.str());
    }

    if(cndn.atTime())
    {
        std::stringstream tmp;
        tmp << "c_eventtime =" << cndn.atTime();
        clause.push_back(tmp.str());
    }

    if(!cndn.identifier().empty())
    {
        std::stringstream tmp;
        tmp << "c_identifier =" << "\"" << cndn.identifier() << "\"";
        clause.push_back(tmp.str());
    }

    std::stringstream query ;
    query << "update "<< CDPV1EVENTTBLNAME 
        << " set " 
        << " c_verified = " << VERIFIEDEVENT <<","
        << " c_comment = " << "\"" << cndn.comment() << "\"";

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
    query << ";";
    querystring = query.str();
    DebugPrintf(SV_LOG_DEBUG,"QUERY to update %s\n",querystring.c_str());
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

//
// FUNCTION NAME :  form_event_query
//
// DESCRIPTION : creates the sql query that matches the condition provided(CDPMarkersMatchingCndn)
//
// INPUT PARAMETERS : CDPMarkersMatchingCndn
//
// OUTPUT PARAMETERS : string - query
//
//
// return value : 
//
//
void CDPV1DatabaseImpl::form_event_query(const CDPMarkersMatchingCndn & cndn, std::string & querystring)
{
    std::vector<std::string> clause;
    if(cndn.type())
    {
        std::stringstream tmp;
        tmp << "c_eventtype =" << cndn.type();
        clause.push_back(tmp.str());
    }

    if(!cndn.value().empty())
    {
        std::stringstream tmp;
        tmp << "c_eventvalue =" << "\"" << cndn.value() << "\"" ;
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
        tmp << "c_eventtime =" << cndn.atTime();
        clause.push_back(tmp.str());
    }

    if(cndn.beforeTime())
    {
        std::stringstream tmp;
        tmp << "c_eventtime <" << cndn.beforeTime();
        clause.push_back(tmp.str());
    }

    if(cndn.afterTime())
    {
        std::stringstream tmp;
        tmp << "c_eventtime >" << cndn.afterTime();
        clause.push_back(tmp.str());
    }

    if(cndn.mode() != ACCURACYANY)
    {
        std::stringstream tmp;
        tmp << "c_eventmode =" << cndn.mode();
        clause.push_back(tmp.str());
    }

    if(cndn.verified() != BOTH_VERIFIED_NONVERIFIED_EVENT)
    {
        std::stringstream tmp;
        tmp << "c_verified =" << cndn.verified();
        clause.push_back(tmp.str());
    }

    std::stringstream query ;
    query << "select * from " << CDPV1EVENTTBLNAME ;

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
        query << " order by c_eventno";
    }
    else if (sortcol == "app" )
    {
        query << " order by c_eventtype";
    }
    else if (sortcol == "event" )
    {
        query << " order by c_eventvalue";
    }
    else if (sortcol == "accuracy")
    {
        query << " order by c_eventmode";
    }
    else // default
    {
        query << " order by c_eventno";
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
}

//
// FUNCTION NAME :  form_timerange_query
//
// DESCRIPTION : creates the sql query that matches the condition provided(CDPTimeRangeMatchingCndn)
//
// INPUT PARAMETERS : CDPTimeRangeMatchingCndn
//
// OUTPUT PARAMETERS : string - query
//
//
// return value : 
//
//
void CDPV1DatabaseImpl::form_timerange_query(const CDPTimeRangeMatchingCndn & cndn, std::string & querystring)
{
    std::vector<std::string> clause;
    if(cndn.mode() != ACCURACYANY)
    {
        std::stringstream tmp;
        tmp << "c_mode =" << cndn.mode();
        clause.push_back(tmp.str());
    }

    std::stringstream query;
    query << "select * from " << CDPV1TIMERANGETBLNAME ;

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

    std::string sortcol = cndn.sortby();
    if(sortcol == "accuracy")
    {
        query << " order by c_mode";
    }
    else
    {
        query << " order by c_entryno";
    }

    std::string sortorder = cndn.sortorder();
    if(sortorder == "default")
    {
        query << " DESC";
    }
    else
    {
        query << " ASC";
    }
    if(cndn.limit())
    {
        query << " limit " << cndn.limit();
    }

    querystring = query.str();
}


SVERROR CDPV1DatabaseImpl::ReadDRTD(cdp_rollback_drtd_t & rollback_drtd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    SVERROR hr = SVS_OK;

    try
    {
        do 
        {
            if(!good())
            {
                hr = SVE_FAIL;
                break;
            }

            if(!m_drtdcndn-> toEvent().empty() && (0 == m_eventinodeno) )
            {
                if(!FetchInodeNoForEvent(m_drtdcndn-> toEvent(), m_drtdcndn-> toTime(), m_eventinodeno))
                {
                    std::stringstream errstr;
                    errstr << FUNCTION_NAME 
                        << ": Inode corresponding to " 
                        << m_drtdcndn-> toEvent() << " wasn't found.\n";

                    DebugPrintf(SV_LOG_ERROR, "%s\n", errstr.str().c_str());
                    hr = SVE_FAIL;
                    good_status = false;
                    break;
                }
            }

            while(true)
            {
                if(!m_inoderead)
                {
                    if(!ReadInode(m_inode, true))
                    {
                        hr = SVS_FALSE;
                        break;
                    }

                    // zero differentials in current inode
                    if(!m_inode.c_numdiffs)
                    {
                        // let's move on to next inode
                        m_inoderead = false;
                        continue;
                    }
                    // the inode start time is greater than the cndn starttime
                    if((m_drtdcndn-> fromTime()) && (m_inode.c_starttime > m_drtdcndn-> fromTime()))
                    {
                        // let's move on to next inode
                        DebugPrintf(SV_LOG_ERROR, 
                            "The inode start time is greater than cndn starttime which should not be happened.\n");
                        m_inoderead = false;
                        continue;
                    }
                    m_diffinfos = m_inode.c_diffinfos;
                    m_diffinfoIndex = m_inode.c_numdiffs - 1;
                    m_inoderead = true;
                    m_drtdsIter = m_diffinfos[m_diffinfoIndex]->changes.rbegin();
                }

                // skip differential which do not contain any changes
                while(m_drtdsIter == m_diffinfos[m_diffinfoIndex]->changes.rend())
                {
                    --m_diffinfoIndex;

                    // reached end of differentials in current inode
                    if(m_diffinfoIndex < 0)
                    {
                        break;
                    }

                    // set drtds iterator to point to next differential within
                    // current inode
                    m_drtdsIter = m_diffinfos[m_diffinfoIndex]->changes.rbegin();
                }

                // all changes from current inode have been read
                if(m_diffinfoIndex < 0)
                {
                    // all changes based on the condition have been processed
                    if(m_inode.c_inodeno <= m_eventinodeno)
                    {
                        hr = SVS_FALSE;
                        break;
                    }

                    // let's move on to next inode
                    m_inoderead = false;
                    continue;
                }

                // process the current change
                cdp_drtdv2_t drtdv2 = *m_drtdsIter;

                if((0 == m_eventinodeno) && 
                    !m_drtdcndn-> toSequenceId() && 
                    m_diffinfos[m_diffinfoIndex]->tslc <= m_drtdcndn-> toTime())
                {
                    hr = SVS_FALSE;
                    break;
                }
                //the current drtd contains the change greater than upper bound, so increamenting the iterator and continuing
                if((m_drtdcndn-> fromTime()) && 
                    ((m_diffinfos[m_diffinfoIndex]->tsfc + drtdv2.get_timedelta()) > m_drtdcndn-> fromTime()))
                {
                    ++m_drtdsIter;
                    continue;
                }
                if((0 == m_eventinodeno)
                    && (m_inode.c_type == NONSPLIT_IO)
                    && !m_drtdcndn-> toSequenceId()
                    && ((m_diffinfos[m_diffinfoIndex]->tsfc + drtdv2.get_timedelta()) <= m_drtdcndn-> toTime() ))
                {
                    hr = SVS_FALSE;//finished reading drtds
                    break;
                }

                if( m_drtdcndn-> toSequenceId()	
                    && ( m_inode.c_type == NONSPLIT_IO )
                    && 
                    (((m_diffinfos[m_diffinfoIndex] -> tsfcseq + drtdv2.get_seqdelta() ) <= m_drtdcndn-> toSequenceId())
                    || ((m_diffinfos[m_diffinfoIndex] -> tsfc + drtdv2.get_timedelta()) < m_drtdcndn-> toTime()))  )
                {
                    hr = SVS_FALSE;//finished reading drtds
                    break;
                }

                rollback_drtd.fileoffset = drtdv2.get_file_offset();
                rollback_drtd.voloffset = drtdv2.get_volume_offset();
                rollback_drtd.length = drtdv2.get_length();
                rollback_drtd.filename = m_inode.c_datafile;
                rollback_drtd.timestamp =
                    m_diffinfos[m_diffinfoIndex] -> tsfc + drtdv2.get_timedelta();
                hr = SVS_OK;
                ++m_drtdsIter;
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
    return hr;
}

//
// FUNCTION NAME :  ReadAppliedDRTD
//
// DESCRIPTION : Reads the next available DRTD from the retention file
//
// INPUT PARAMETERS : none
//
// OUTPUT PARAMETERS : cdp_rollback_drtdptr_t
//
// ALGORITHM : a. Parses the retention file
//             b. retrieves the next available DRTD
//			   c. offsets/length are filled from the retention file,
//					filename is filled from the AppliedInformation
//
// return value : SVERROR - SVS_OK - if success (and more drtds are present)
//							SVS_FALSE - if success ( and no further drtds are present)
//							SVE_FAIL - failure in reading drtds
//
SVERROR CDPV1DatabaseImpl::ReadAppliedDRTD(cdp_rollback_drtd_t & rollback_drtd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

    SVERROR hr = SVS_OK;

    try
    {
        do 
        {
            if(!good())
            {
                hr = SVE_FAIL;
                break;
            }

            if(!m_partialtxnread)
            {

                if(!ReadInode(m_inode, false))
                {
                    hr = SVS_FALSE;
                    break;
                }

                if(!CDPUtil::get_last_retentiontxn(m_dbdir, m_lastRetentionTxn))
                {
                    hr = SVE_FAIL;
                    break;
                }

                if(0 == m_lastRetentionTxn.partialTransaction.noOfChangesApplied)
                {
                    hr = SVS_FALSE;
                    break;
                }

                std::string cdpDataFilePath = m_inode.c_datadir;
                cdpDataFilePath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                cdpDataFilePath += m_inode.c_datafile;

                DifferentialFile onDiskParser(cdpDataFilePath);
                //onDiskParser.SetBaseline(m_baseline);
                onDiskParser.SetValidation(false);
                onDiskParser.SetStartOffset(m_inode.c_logicalsize);


                if( SV_SUCCESS != onDiskParser.Open(BasicIo::BioReadExisting |
                    BasicIo::BioShareRW | BasicIo::BioBinary) )
                {
                    DebugPrintf(SV_LOG_ERROR, 
                        "Failed to open file %s for rolling back partial transaction. errorno=%d\n",
                        cdpDataFilePath.c_str(), ACE_OS::last_error());
                    good_status = false;
                    hr = SVE_FAIL;
                    break;
                }

                m_metadata.reset(new Differential);

                if(!onDiskParser.ParseDiff(m_metadata))
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to parse the partial transaction in file %s.\n",
                        cdpDataFilePath.c_str());
                    good_status = false;
                    hr = SVE_FAIL;
                    break;
                }

                if(m_lastRetentionTxn.partialTransaction.noOfChangesApplied > m_metadata->NumDrtds())
                {
                    DebugPrintf(SV_LOG_ERROR, "Either the metadata or appliedinfo is corrupted.\n");
                    good_status = false;
                    hr = SVE_FAIL;
                    break;
                }

                m_change = m_lastRetentionTxn.partialTransaction.noOfChangesApplied;
                m_partialtxnread = true;
            }

            if(m_change <= 0)
            {
                hr = SVS_FALSE;
                break;
            }

            cdp_drtdv2_t metadata_drtd = m_metadata -> drtd(m_change -1);
            rollback_drtd.filename = m_inode.c_datafile;
            rollback_drtd.fileoffset = metadata_drtd.get_file_offset();
            rollback_drtd.length = metadata_drtd.get_length();
            rollback_drtd.voloffset = metadata_drtd.get_volume_offset();
            rollback_drtd.timestamp = 
                m_metadata -> starttime() + metadata_drtd.get_timedelta();

            m_change--;
            hr = SVS_OK;

        } while (0);

    }catch (std::exception & ex) {
        std::stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured while reading Applied DRTD\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*(m_con)).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        good_status = false;
        hr = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    return hr;
}


//
// FUNCTION NAME :  FetchInodeNoForEvent
//
// DESCRIPTION : Retrieves the inode number from the event table for the specified bookmark
//
// INPUT PARAMETERS : 1. string - bookmark/event value
//                    2. SV_ULONGLONG - eventtime (recoverytime of the event)
//                    
//
// OUTPUT PARAMETERS : inodeno - inode number associated with the event 
//
//
// return value : true if success, else false
//
//
bool CDPV1DatabaseImpl::FetchInodeNoForEvent(const std::string & eventvalue, const SV_ULONGLONG & eventtime, SV_ULONGLONG & inodeNo ) 
{ 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME); 
    bool rv = false; 

    do { 
        try 
        { 
            std::stringstream query ; 
            query << "select c_inodeno from " << CDPV1EVENTTBLNAME 
                << " where c_eventvalue=" << '?' 
                << " AND c_eventtime=" << eventtime ; 

            boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));


            cmd -> bind( 1, eventvalue ); 
            sqlite3_reader rdr = cmd -> executereader(); 
            if(!rdr.read()) 
            { 
                rdr.close();
                rv = false; 
                break; 
            }

            inodeNo = rdr.getint64(0); 
            rdr.close();
            rv = true; 
        } 
        catch(exception &ex) 
        { 
            std::stringstream l_stderr; 
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n" 
                << "Exception Occured during read operation\n" 
                << " Database : " << m_dbname << "\n" 
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*m_con).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ; 

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str()); 
            rv = false; 
        } 
    } while (0); 

    return rv; 
} 

//
// FUNCTION NAME :  FetchOldestInode
//
// DESCRIPTION : Reads the oldest inode from the database
//
// INPUT PARAMETERS : ReadDiff - bool. If this is true differentials for the inode will be read
//
// OUTPUT PARAMETERS : CDPV1Inode
//  
//
// return value : true if inode is read, false if there is no inode.
//
bool CDPV1DatabaseImpl::FetchOldestInode( CDPV1Inode & inode, bool ReadDiff )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    do 
    {
        stringstream query ;
        query << "select * from " << CDPV1INODETBLNAME << " order by c_inodeno ASC limit 1";
        sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());

        m_cmd.reset(cmd);
        m_reader = m_cmd -> executereader();

        if(!ReadInode(inode, ReadDiff))
        {
            rv = false;
            break;
        }

        m_reader.close();


    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

//
// FUNCTION NAME :  FetchLatestInode
//
// DESCRIPTION : Reads the latest inode from the database
//
// INPUT PARAMETERS : ReadDiff - bool. If this is true differentials for the inode will be read
//
// OUTPUT PARAMETERS : CDPV1Inode
//  
//
// return value : true if inode is read, false if there is no inode.
//
bool CDPV1DatabaseImpl::FetchLatestInode( CDPV1Inode & inode, bool ReadDiff )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    bool rv = true;

    do 
    {
        stringstream query ;
        query << "select * from " << CDPV1INODETBLNAME << " order by c_inodeno DESC limit 1";
        sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());

        m_cmd.reset(cmd);
        m_reader = m_cmd -> executereader();

        if(!ReadInode(inode, ReadDiff))
        {
            rv = false;
            break;
        }

        m_reader.close();


    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


//
// FUNCTION NAME :  DeleteInode
//
// DESCRIPTION : Deletes the inode
//
// INPUT PARAMETERS : inode - CDPV1Inode, inode to be deleted
//
// OUTPUT PARAMETERS : None
//  
// return value : None
//
void CDPV1DatabaseImpl::DeleteInode( const CDPV1Inode & inode )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

    stringstream action;
    action << "delete from "  << CDPV1INODETBLNAME 
        << " where c_inodeno="
        << inode.c_inodeno
        << ";" ;

    sqlite3_command cmd(*m_con, action.str() );
    cmd.executenonquery();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
}
//
// FUNCTION NAME :  FetchEventsForInode
//
// DESCRIPTION : Retrieves the list of events for the specified inode
//
// INPUT PARAMETERS : CDPV1Inode
//
// OUTPUT PARAMETERS : list<CDPEvent> - list of events
//  
// return value : None
//
void CDPV1DatabaseImpl::FetchEventsForInode( const CDPV1Inode & inode, CDPEvents & events)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

    stringstream query ;
    query << "select * from " << CDPV1EVENTTBLNAME 
        << " where c_inodeno=" << inode.c_inodeno;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());

    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    CDPEvent cdpevent;
    while(ReadEvent(cdpevent))
    {
        events.push_back(cdpevent);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
}
//
// FUNCTION NAME :  DeleteEvent
//
// DESCRIPTION : Deletes the event from the event table
//
// INPUT PARAMETERS : CDPEvent
//
// OUTPUT PARAMETERS : None
//  
// return value : None
//
void CDPV1DatabaseImpl::DeleteEvent( const CDPEvent& cdpevent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

    stringstream action;
    action << "delete from "  << CDPV1EVENTTBLNAME 
        << " where c_eventno="
        << cdpevent.c_eventno
        << ";" ;

    sqlite3_command cmd(*m_con, action.str() );
    cmd.executenonquery();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
}

//
// FUNCTION NAME :  InsertPendingEvent
//
// DESCRIPTION : Inserts the specified event in PendingEvent Table
//
// INPUT PARAMETERS : CDPV1PendingEvent
//
// OUTPUT PARAMETERS : None
//  
// return value : None
//
void CDPV1DatabaseImpl::InsertPendingEvent( const CDPV1PendingEvent& event)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

    stringstream  action;
    action << "insert into " << CDPV1PENDINGEVENTTBLNAME
        << " ( c_inodeno,c_eventtype,c_eventtime,c_eventvalue,c_valid ,c_eventmode,c_identifier,c_verified,c_comment)"
        << " values("
        << event.c_inodeno     << ","
        << event.c_eventtype   << ","
        << event.c_eventtime   << ","
        << '?' << ","
        << event.c_valid << ","
        << event.c_eventmode << ","
        << '?' << ","
        << event.c_verified << ","
        << '?' << " );" ;

    sqlite3_command cmd(*m_con, action.str() );
    cmd.bind( 1, event.c_eventvalue );
    cmd.bind(2, event.c_identifier);
    cmd.bind(3, event.c_comment);
    cmd.executenonquery();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
}
//
// FUNCTION NAME :  UpdateSuperBlock
//
// DESCRIPTION : Updates the superblock
//
// INPUT PARAMETERS : CDPV1SuperBlock
//
// OUTPUT PARAMETERS : None
//  
// return value : None
//
void CDPV1DatabaseImpl::UpdateSuperBlock( const CDPV1SuperBlock & superblock)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
    EventSummary_t::const_iterator events_iter = superblock.c_eventsummary.begin();
    EventSummary_t::const_iterator events_end = superblock.c_eventsummary.end();

    stringstream eventsummary;
    for ( ; events_iter != events_end ; ++events_iter)
    {
        SV_EVENT_TYPE EventType = events_iter ->first;
        SV_ULONGLONG NumEvents = events_iter ->second;
        eventsummary << EventType << "," << NumEvents << ";" ;
    }


    stringstream action;

    action << "update " << CDPV1SUPERBLOCKTBLNAME
        << " set "
        << " c_lookuppagesize=" << superblock.c_lookuppagesize
        << " ," << " c_physicalspace=" << superblock.c_physicalspace 
        << " ," << " c_tsfc=" << superblock.c_tsfc
        << " ," << " c_tslc=" << superblock.c_tslc
        << " ," << " c_numinodes=" << superblock.c_numinodes
        << " ," << " c_eventsummary=" << "'" << eventsummary.str() << "'"
        << ";" ;

    sqlite3_command cmd(*m_con, action.str() );
    cmd.executenonquery();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

}
//
// FUNCTION NAME :  AdjustTimeRangeTable
//
// DESCRIPTION : Adjust the TimeRange Table
//
// INPUT PARAMETERS : superblock - CDPV1SuperBlock
//
// OUTPUT PARAMETERS : None
//  
// return value : None
//
void CDPV1DatabaseImpl::AdjustTimeRangeTable( const CDPV1SuperBlock & superblock)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());


    // 1 Delete if end time < first time
    // 2 Update start time = first time
    //          where  start time < first time and  end time > first time

    stringstream delquery1,delquery2;
    delquery1 << " delete from "<< CDPV1TIMERANGETBLNAME ;
    if(superblock.c_tsfc)
    {
        delquery1 << " where c_endtime <" << superblock.c_tsfc;
    }

    sqlite3_command del_cmd1(*m_con, delquery1.str() );
    del_cmd1.executenonquery();

    delquery2 << " delete from "<< CDPV1PENDINGTIMERANGETBLNAME;
    if(superblock.c_tsfc)
    {
        delquery2 << " where c_endtime <" << superblock.c_tsfc;
    }

    sqlite3_command del_cmd2(*m_con, delquery2.str() );
    del_cmd2.executenonquery();

    stringstream updquery1,updquery2;
    updquery1 << "update " << CDPV1TIMERANGETBLNAME
        << " set "
        << " c_starttime = " << superblock.c_tsfc 					                  
        << " where c_starttime < " << superblock.c_tsfc << " and "
        << " c_endtime > " << superblock.c_tsfc << ";" ;

    sqlite3_command update_cmd1(*m_con, updquery1.str() );
    update_cmd1.executenonquery();

    updquery2 << "update " << CDPV1PENDINGTIMERANGETBLNAME
        << " set "
        << " c_starttime = " << superblock.c_tsfc 					                  
        << " where c_starttime < " << superblock.c_tsfc << " and "
        << " c_endtime > " << superblock.c_tsfc << ";" ;

    sqlite3_command update_cmd2(*m_con, updquery2.str() );
    update_cmd2.executenonquery();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
}

//
// FUNCTION NAME :  ReadEvent
//
// DESCRIPTION : Reads the next available event from the database according to the query set
//
//
// OUTPUT PARAMETERS : CDPV1Event
//
//
// return value : true if inode is read, else false
//				
bool CDPV1DatabaseImpl::ReadEvent(CDPEvent & cdpevent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {

        if(!m_reader.read())
        {
            rv = false;
            break;
        }

        cdpevent.c_eventno = m_reader.getint64(EVENT_C_EVENTNO_COL);
        cdpevent.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_EVENTTYPE_COL));
        cdpevent.c_eventtime = m_reader.getint64(EVENT_C_EVENTTIME_COL);
        cdpevent.c_eventvalue = m_reader.getblob(EVENT_C_EVENTVALUE_COL); 
        cdpevent.c_eventmode = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_EVENTMODE_COL));
        CDPVersion ver;
        GetRevision(ver);
        if(ver.c_revision >= CDPVER1REV3)
        {
            cdpevent.c_identifier = m_reader.getblob(EVENT_C_IDENTIFIER_COL);
            cdpevent.c_verified = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_VERIFIED_COL));
            cdpevent.c_comment = m_reader.getblob(EVENT_C_COMMENT_COL);
            if(cdpevent.c_verified == VERIFIEDEVENT)
            {
                cdpevent.c_status = CDP_BKMK_UPDATE_REQUEST;
            } else
            {
                cdpevent.c_status = CDP_BKMK_INSERT_REQUEST;
            }
        }else
        {
            cdpevent.c_identifier = "";
            cdpevent.c_verified = NONVERIFIEDEVENT;
            cdpevent.c_comment = "";
            cdpevent.c_status = CDP_BKMK_INSERT_REQUEST;
        }
        cdpevent.c_lockstatus = BOOKMARK_STATE_UNLOCKED;
        cdpevent.c_lifetime = INM_BOOKMARK_LIFETIME_NOTSPECIFIED;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

//
// FUNCTION NAME :  ReadPendingEvent
//
// DESCRIPTION : Reads the next available event from the database according to the query set
//
//
// OUTPUT PARAMETERS : CDPV1Event
//
//
// return value : true if inode is read, else false
//				
bool CDPV1DatabaseImpl::ReadPendingEvent(CDPEvent & cdpevent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    SV_UINT status = 0;

    do
    {

        if(!m_reader.read())
        {
            rv = false;
            break;
        }

        cdpevent.c_eventno = m_reader.getint64(PENDINGEVENT_C_EVENTNO_COL);
        cdpevent.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(PENDINGEVENT_C_EVENTTYPE_COL));
        cdpevent.c_eventtime = m_reader.getint64(PENDINGEVENT_C_EVENTTIME_COL);
        cdpevent.c_eventvalue = m_reader.getblob(PENDINGEVENT_C_EVENTVALUE_COL); 
        cdpevent.c_eventmode = static_cast<SV_USHORT> (m_reader.getint(PENDINGEVENT_C_EVENTMODE_COL));
        CDPVersion ver;
        GetRevision(ver);
        if(ver.c_revision >= CDPVER1REV3)
        {
            cdpevent.c_identifier = m_reader.getblob(PENDINGEVENT_C_IDENTIFIER_COL);
            cdpevent.c_verified = static_cast<SV_USHORT> (m_reader.getint(PENDINGEVENT_C_VERIFIED_COL));
            cdpevent.c_comment = m_reader.getblob(PENDINGEVENT_C_COMMENT_COL);
            status = m_reader.getint(PENDINGEVENT_C_VALID_COL);
            if(status == 0)
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
        }else
        {
            cdpevent.c_identifier = "";
            cdpevent.c_verified = NONVERIFIEDEVENT;
            cdpevent.c_comment = "";
            cdpevent.c_status = CDP_BKMK_INSERT_REQUEST;
        }
        cdpevent.c_lockstatus = BOOKMARK_STATE_UNLOCKED;
        cdpevent.c_lifetime = INM_BOOKMARK_LIFETIME_NOTSPECIFIED;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1DatabaseImpl::ReadTimeRange(CDPTimeRange & timerange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {

        if(!m_reader.read())
        {
            rv = false;
            break;
        }

        timerange.c_entryno = m_reader.getint64(TIMERANGE_C_ENTRYNO_COL);
        timerange.c_starttime = m_reader.getint64(TIMERANGE_C_STARTTIME_COL);
        timerange.c_mode = static_cast<SV_USHORT> (m_reader.getint(TIMERANGE_C_MODE_COL));
        timerange.c_endtime = m_reader.getint64(TIMERANGE_C_ENDTIME_COL);
        timerange.c_startseq =  m_reader.getint64(TIMERANGE_C_STARTSEQ_COL);
        timerange.c_endseq =  m_reader.getint64(TIMERANGE_C_ENDSEQ_COL);
        timerange.c_granularity = CDP_ANY_POINT_IN_TIME;
        timerange.c_action = CDP_TIMERANGE_INSERT_REQUEST;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1DatabaseImpl::ReadPendingTimeRange(CDPTimeRange & timerange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {

        if(!m_reader.read())
        {
            rv = false;
            break;
        }

        timerange.c_entryno = m_reader.getint64(PENDINGTIMERANGE_C_ENTRYNO_COL);
        timerange.c_starttime = m_reader.getint64(PENDINGTIMERANGE_C_STARTTIME_COL);
        timerange.c_mode = static_cast<SV_USHORT> (m_reader.getint(PENDINGTIMERANGE_C_MODE_COL));
        timerange.c_endtime = m_reader.getint64(PENDINGTIMERANGE_C_ENDTIME_COL);
        timerange.c_startseq =  m_reader.getint64(PENDINGTIMERANGE_C_STARTSEQ_COL);
        timerange.c_endseq =  m_reader.getint64(PENDINGTIMERANGE_C_ENDSEQ_COL);
        timerange.c_granularity = CDP_ANY_POINT_IN_TIME;
        timerange.c_action = CDP_TIMERANGE_INSERT_REQUEST;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
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
bool CDPV1DatabaseImpl::get_retention_info(CDPInformation & info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
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
            << "Exception Occured while retrieving the summary from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


void CDPV1DatabaseImpl::getcdptimeranges_nolock(CDPTimeRanges & ranges)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

    std::stringstream query;
    query << "select * from " << CDPV1TIMERANGETBLNAME ;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    CDPTimeRange timerange;
    while(ReadTimeRange(timerange))
    {
        ranges.push_back(timerange);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV1DatabaseImpl::getcdpevents_nolock(CDPEvents & events)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

    std::stringstream query;
    query << "select * from " << CDPV1EVENTTBLNAME ;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    CDPEvent event;
    while(ReadEvent(event))
    {
        events.push_back(event);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
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
bool CDPV1DatabaseImpl::get_pending_updates(CDPInformation & info,const SV_ULONG & record_limit)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
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
            << "Exception Occured while retrieving the summary from the database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

void CDPV1DatabaseImpl::getcdppendingtimeranges_nolock(CDPTimeRanges & ranges,const SV_ULONG & records_to_fetch)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

    std::stringstream query;
    query << "select * from " << CDPV1PENDINGTIMERANGETBLNAME ;
    if(records_to_fetch)
        query  << " limit " << records_to_fetch ;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    CDPTimeRange timerange;
    while(ReadPendingTimeRange(timerange))
    {
        ranges.push_back(timerange);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV1DatabaseImpl::getcdppendingevents_nolock(CDPEvents & events,const SV_ULONG & records_to_fetch)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

    std::stringstream query;
    query << "select * from " << CDPV1PENDINGEVENTTBLNAME  ;
    if(records_to_fetch)
        query << " limit " << records_to_fetch;

    sqlite3_command * cmd = new sqlite3_command(*(m_con),query.str());
    m_cmd.reset(cmd);
    m_reader = m_cmd -> executereader();

    CDPEvent event;
    while(ReadPendingEvent(event))
    {
        events.push_back(event);
    }

    m_reader.close();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

//
// FUNCTION NAME :  delete_pending_updates
//
// DESCRIPTION : delete updates that are sent to cs
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
bool CDPV1DatabaseImpl::delete_pending_updates(CDPInformation & info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
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
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


void CDPV1DatabaseImpl::deletecdppendingtimeranges_nolock(CDPTimeRanges & ranges)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

    CDPTimeRanges::iterator timeranges_iter = ranges.begin();
    CDPTimeRanges::iterator timeranges_end = ranges.end();

    for( ; timeranges_iter != timeranges_end ; ++timeranges_iter)
    {
        CDPTimeRange timerange = *timeranges_iter;

        std::stringstream delquery;
        delquery << "delete from " << CDPV1PENDINGTIMERANGETBLNAME 
            << " where c_entryno = " 
            << timerange.c_entryno;

        sqlite3_command del_cmd(*m_con, delquery.str() );
        del_cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}


void CDPV1DatabaseImpl::deletecdppendingevents_nolock(CDPEvents & events)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

    CDPEvents::iterator events_iter = events.begin();
    CDPEvents::iterator events_end = events.end();

    for( ; events_iter != events_end ; ++events_iter)
    {
        CDPEvent curr_event = *events_iter;

        std::stringstream delquery;
        delquery << "delete from " << CDPV1PENDINGEVENTTBLNAME 
            << " where c_eventno = " 
            << curr_event.c_eventno;

        sqlite3_command del_cmd(*m_con, delquery.str() );
        del_cmd.executenonquery();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

//
// FUNCTION NAME :  delete_all_pending_updates
//
// DESCRIPTION : delete all updates (assuming cs/sse does not need to be sent)
//
//
// INPUT PARAMETERS : none
//                  
// 
// OUTPUT PARAMETERS : none
//
// return value : true if success, else false
//
//
bool CDPV1DatabaseImpl::delete_all_pending_updates()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
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
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
        rv = false;
        shutdown();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

void CDPV1DatabaseImpl::deleteallpendingtimeranges_nolock()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

	std::stringstream delquery;
	delquery << "delete from " << CDPV1PENDINGTIMERANGETBLNAME ;
	sqlite3_command del_cmd(*m_con, delquery.str() );
	del_cmd.executenonquery();

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

void CDPV1DatabaseImpl::deleteallpendingevents_nolock()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s \n",FUNCTION_NAME, m_dbname.c_str());

	std::stringstream delquery;
	delquery << "delete from " << CDPV1PENDINGEVENTTBLNAME ;

	sqlite3_command del_cmd(*m_con, delquery.str() );
	del_cmd.executenonquery();

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, m_dbname.c_str());
}

bool CDPV1DatabaseImpl::upgrade()
{
    bool rv = true;
    CDPVersion ver;
    GetRevision(ver);
    if(ver.c_revision == CDPVER1REV0)
    {
        if(!UpgradeRev0())
        {
            rv = false;
        }
    }
    else if(ver.c_revision == CDPVER1REV1)
    {
        if(!UpgradeRev1())
        {
            rv = false;
        }
    }
    else if(ver.c_revision == CDPVER1REV2)
    {
        if(!UpgradeRev2())
        {
            rv = false;
        }
    }
    return rv;
}

bool CDPV1DatabaseImpl::UpgradeRev2()
{
    bool rv = true;

    try
    {
        std::stringstream update_version_table ;
        std::stringstream upgrade_event_table ;
        std::stringstream upgrade_pendingevent_table ;
        std::stringstream upgrade_pendingevent_table_comment ;
        std::stringstream upgrade_event_table_comment ;
        std::stringstream upgrade_pendingevent_table_verified ;
        std::stringstream upgrade_event_table_verified ;
        std::vector<std::string> stmts;

        upgrade_event_table << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";

        stmts.push_back(upgrade_event_table.str());

        upgrade_event_table_verified << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";

        stmts.push_back(upgrade_event_table_verified.str());

        upgrade_event_table_comment << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";

        stmts.push_back(upgrade_event_table_comment.str());

        upgrade_pendingevent_table << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";

        stmts.push_back(upgrade_pendingevent_table.str());

        upgrade_pendingevent_table_verified << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";

        stmts.push_back(upgrade_pendingevent_table_verified.str());

        upgrade_pendingevent_table_comment << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";

        stmts.push_back(upgrade_pendingevent_table_comment.str());

        update_version_table << "update "<< CDPV1VERSIONTBLNAME 
            << " set " 
            << "c_version = "<< CDPVER <<","
            <<" c_revision = "<< CDPVERCURRREV << ";";

        stmts.push_back(update_version_table.str());

        executestmts(stmts);

    }catch (exception &ex)	{
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database :" 
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }
    return rv;
}

bool CDPV1DatabaseImpl::UpgradeRev0()
{
    bool rv = true;

    try
    {
        std::stringstream update_version_table ;
        std::stringstream upgrade_event_table_mode ;
        std::stringstream upgrade_event_table_idenifier ;
        std::stringstream upgrade_pendingevent_table_mode ;
        std::stringstream upgrade_pendingevent_table_identifier ;
        std::stringstream upgrade_pendingevent_table_comment ;
        std::stringstream upgrade_event_table_comment ;
        std::stringstream upgrade_pendingevent_table_verified ;
        std::stringstream upgrade_event_table_verified ;
        std::vector<std::string> stmts;

        upgrade_event_table_mode << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_eventmode INTEGER DEFAULT 1 " ;
        stmts.push_back(upgrade_event_table_mode.str());

        upgrade_event_table_idenifier << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";
        stmts.push_back(upgrade_event_table_idenifier.str());

        upgrade_event_table_verified << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        stmts.push_back(upgrade_event_table_verified.str());

        upgrade_event_table_comment << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        stmts.push_back(upgrade_event_table_comment.str());

        upgrade_pendingevent_table_mode << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_eventmode INTEGER DEFAULT 1 " ;
        stmts.push_back(upgrade_pendingevent_table_mode.str());

        upgrade_pendingevent_table_identifier << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";
        stmts.push_back(upgrade_pendingevent_table_identifier.str());

        upgrade_pendingevent_table_verified << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        stmts.push_back(upgrade_pendingevent_table_verified.str());

        upgrade_pendingevent_table_comment << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        stmts.push_back(upgrade_pendingevent_table_comment.str());

        CDPV1SuperBlock superblock;
        ReadSuperBlock(superblock);
        if(superblock.c_tsfc && superblock.c_tslc)
        {
            if(isempty(CDPV1TIMERANGETBLNAME))
            {
                stringstream  action;
                action << "insert into " << CDPV1TIMERANGETBLNAME
                    << " ( c_starttime,c_endtime,c_mode,c_startseq,c_endseq )"
                    << " values("
                    << superblock.c_tsfc  << ","
                    << superblock.c_tslc  << ","
                    << 1 << ","	<< 0 << ","	<< 0 << " );" ;
                stmts.push_back(action.str());
            }
            if(isempty(CDPV1PENDINGTIMERANGETBLNAME))
            {
                stringstream  action;
                action << "insert into " << CDPV1PENDINGTIMERANGETBLNAME
                    << " ( c_starttime,c_endtime,c_mode,c_startseq,c_endseq )"
                    << " values("
                    << superblock.c_tsfc  << ","
                    << superblock.c_tslc  << ","
                    << 1 << ","	<< 0 << ","	<< 0 << " );" ;
                stmts.push_back(action.str());
            }
        }

        stringstream update_inode_table;
        update_inode_table << " ALTER TABLE " << CDPV1INODETBLNAME << " ADD COLUMN " 
            << " c_version INTEGER DEFAULT " << CDPV1_INODE_VERSION0 ;
        stmts.push_back(update_inode_table.str());

        update_version_table << "update "<< CDPV1VERSIONTBLNAME 
            << " set " 
            << "c_version = "<< CDPVER <<","
            <<" c_revision = "<< CDPVERCURRREV << ";";

        stmts.push_back(update_version_table.str());

        executestmts(stmts);

    }catch (exception &ex)	{
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database :" 
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }
    return rv;
}

bool CDPV1DatabaseImpl::UpgradeRev1()
{
    bool rv = true;

    try
    {
        std::stringstream update_version_table ;
        std::stringstream upgrade_event_table_idenifier ;
        std::stringstream upgrade_pendingevent_table_identifier ;
        std::stringstream upgrade_pendingevent_table_comment ;
        std::stringstream upgrade_event_table_comment ;
        std::stringstream upgrade_pendingevent_table_verified ;
        std::stringstream upgrade_event_table_verified ;
        std::vector<std::string> stmts;

        stringstream update_inode_table;
        update_inode_table << " ALTER TABLE " << CDPV1INODETBLNAME << " ADD COLUMN " 
            << " c_version INTEGER DEFAULT " << CDPV1_INODE_VERSION0 ;
        stmts.push_back(update_inode_table.str());

        stringstream update_timerange_table_startseq;
        update_timerange_table_startseq << " ALTER TABLE " << CDPV1TIMERANGETBLNAME << " ADD COLUMN " 
            << " c_startseq INTEGER DEFAULT 0 " ;
        stmts.push_back(update_timerange_table_startseq.str());

        stringstream update_timerange_table_endseq;
        update_timerange_table_endseq << " ALTER TABLE " << CDPV1TIMERANGETBLNAME << " ADD COLUMN "
            << " c_endseq INTEGER DEFAULT 0 ";
        stmts.push_back(update_timerange_table_endseq.str());

        stringstream update_pendingtimerange_table_startseq;
        update_pendingtimerange_table_startseq << " ALTER TABLE " << CDPV1PENDINGTIMERANGETBLNAME << " ADD COLUMN " 
            << " c_startseq INTEGER DEFAULT 0 " ;
        stmts.push_back(update_pendingtimerange_table_startseq.str());

        stringstream update_pendingtimerange_table_endseq;
        update_pendingtimerange_table_endseq << " ALTER TABLE " << CDPV1PENDINGTIMERANGETBLNAME << " ADD COLUMN "
            << " c_endseq INTEGER DEFAULT 0 ";
        stmts.push_back(update_pendingtimerange_table_endseq.str());

        upgrade_event_table_idenifier << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";
        stmts.push_back(upgrade_event_table_idenifier.str());

        upgrade_event_table_verified << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        stmts.push_back(upgrade_event_table_verified.str());

        upgrade_event_table_comment << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        stmts.push_back(upgrade_event_table_comment.str());

        upgrade_pendingevent_table_identifier << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";
        stmts.push_back(upgrade_pendingevent_table_identifier.str());

        upgrade_pendingevent_table_verified << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        stmts.push_back(upgrade_pendingevent_table_verified.str());

        upgrade_pendingevent_table_comment << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        stmts.push_back(upgrade_pendingevent_table_comment.str());

        update_version_table << "update "<< CDPV1VERSIONTBLNAME 
            << " set " 
            << "c_version = "<< CDPVER <<","
            <<" c_revision = "<< CDPVERCURRREV << ";";

        stmts.push_back(update_version_table.str());

        executestmts(stmts);

    }catch (exception &ex)	{
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database :" 
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }
    return rv;
}
