//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpv1database.cpp
//
// Description: 
//

#include "cdpv1database.h"
#include "cdpv1globals.h"
#include "cdputil.h"
//#include "cdpsettings.h"
#include "cdpplatform.h"
#include "error.h"
#include "localconfigurator.h"

#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/Time_Value.h>
#include <sstream>
#include <iomanip>
#include "inmsafecapis.h"


using namespace std;
using namespace sqlite3x;

const static SV_UINT SV_DEFAULT_PAGES_IN_CDPDB_CACHE = 16384;

CDPV1Database::CDPV1Database(const string & dbpath)
:m_dbname(dbpath)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    // PR#10815: Long Path support
    std::vector<char> vdb_dir(SV_MAX_PATH, '\0');
    DirName(m_dbname.c_str(), &vdb_dir[0], vdb_dir.size());
    PathRemoveAllBackslashes(&vdb_dir[0]);

    m_dbdir =  std::string(vdb_dir.data());
    m_dbdir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    m_versionAvailableInCache = false;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

CDPV1Database::~CDPV1Database()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    Disconnect();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}



bool CDPV1Database::Exists()
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


bool CDPV1Database::Connect()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        if (m_con)
        {
            rv = true;
            break;
        }

        try
        {
            // PR#10815: Long Path support
            sqlite3_connection * con = new (nothrow) sqlite3_connection(getLongPathName(m_dbname.c_str()).c_str());
            if(!con)
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error: insufficient memory to allocate CDP Database connnection Object.\n"
                    << "Database Name:" << m_dbname << "\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }

            m_con.reset(con);
            m_con -> setbusyhandler(CDPUtil::cdp_busy_handler, NULL);
            rv = true;
        }
        catch(exception &ex) {

            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured while establishing database connection to :" 
                << m_dbname << "\n" 
                << "Exception :" << ex.what() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Disconnect()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        if(m_trans)
        {
            m_trans ->rollback();
            m_trans.reset();
        }

        m_con.reset();

        rv = true;
    }
    catch(exception &ex) {

        stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while closing database connection to:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::BeginTransaction()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        try
        {
            if(!m_con)
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error: Database is disconnected.\n"
                    << "Database Name:" << m_dbname << "\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }

            sqlite3_transaction * trans = new (nothrow) sqlite3_transaction(*m_con, true,sqlite3_transaction::sqlite3txn_immediate);
            if(!trans)
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error: insufficient memory to allocate CDP transaction Object.\n"
                    << "Database Name:" << m_dbname << "\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }

            m_trans.reset(trans);
            rv = true;
        }
        catch(exception &ex) {

            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured in begin transaction on database:" 
                << m_dbname << "\n" 
                << "Exception :" << ex.what() << "\n";

            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
        }
    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV1Database::CommitTransaction()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        if(!m_trans)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Error: No active Transaction.\n"
                << "Database Name:" << m_dbname << "\n";

            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
        } else
        {
            m_trans ->commit();
            m_trans.reset();
            rv = true;
        }
    }
    catch(exception &ex) {

        stringstream l_stdfatal;
        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while commiting transaction on database:" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n";

        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


SVERROR CDPV1Database::TableExists(string tablename)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = SVE_FAIL;

    try
    {
        stringstream query ;
        query  << "select count(*) from sqlite_master where name='" << tablename << "'";
        int count=(*m_con).executeint(query.str());
        if(!count)
            rv = SVS_FALSE;
        else
            rv = SVS_OK;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while checking for existence of " << tablename << "table in database :" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SVERROR CDPV1Database::VersionTblExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1VERSIONTBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SVERROR CDPV1Database::isempty(string table)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = SVS_OK;

    try 
    {
        do
        {
            stringstream query;
            query << "select 1 from " << table <<" limit 1" ;

            boost::shared_ptr<sqlite3_command> cmd(new (nothrow) sqlite3_command(*m_con, query.str()));
            if(!cmd)
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
                    << "Error: insufficient memory to allocate Command Object.\n"
                    << "Database Name:" << m_dbname << "\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                rv = SVE_FAIL;
                break;
            }

            sqlite3_reader reader= cmd -> executereader();

            if(reader.read())
            {
                rv = SVS_FALSE;
                break;
            }

        } while (0); 
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while checking whether table is empty or not\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << table    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv ;
}

bool CDPV1Database::CreateVersionTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1VERSIONTBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_VERSIONTBL_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_VERSION_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while while creating version table for database :" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::InitializeVersionTable(const CDPVersion & dbver)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action ;
        action << "insert into " << CDPV1VERSIONTBLNAME 
            << " ( c_version, c_revision )"
            << " values(" 
            << dbver.c_version << ","
            << dbver.c_revision
            << ");" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while while initializing version for database :" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::read(CDPVersion & dbver)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    if(m_versionAvailableInCache)
    {
        dbver.c_version = m_ver.c_version;
        dbver.c_revision = m_ver.c_revision;
    }

    stringstream query ;
    query << "select * from " << CDPV1VERSIONTBLNAME  << " ; ";

    do
    {
        try
        {

            boost::shared_ptr<sqlite3_command> cmd(new (nothrow) sqlite3_command(*m_con, query.str()));
            if(!cmd)
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
                    << "Error: insufficient memory to allocate Command Object.\n"
                    << "Database Name:" << m_dbname << "\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }

            sqlite3_reader reader= cmd -> executereader();

            if(!reader.read())
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error Occured while while reading version information.\n"
                    << "Database Name:" << m_dbname << "\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }

            dbver.c_version = reader.getdouble(VERSIONTBL_C_VERSION_COL);
            dbver.c_revision = reader.getdouble(VERSIONTBL_C_REVISION_COL);

            m_ver.c_version = dbver.c_version;
            m_ver.c_revision = dbver.c_revision;
            m_versionAvailableInCache = true;
            rv = true;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured while while reading version information\n"
                << " Database : " << m_dbname << "\n"
                << " Table : " << CDPV1VERSIONTBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
        }

    } while ( FALSE );


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


//to upgrade version table
bool CDPV1Database::write(CDPVersion & dbver)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try{
        stringstream query ;
        query << "update "<< CDPV1VERSIONTBLNAME 
            << " set " 
            << "c_version = "<< dbver.c_version <<","
            <<" c_revision = "<< dbver.c_revision << ";";

        (*m_con).executenonquery(query.str());

        m_ver.c_version = dbver.c_version;
        m_ver.c_revision = dbver.c_revision;
        m_versionAvailableInCache = true;

        rv=true;

    }catch (exception &ex)	{
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1VERSIONTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



bool CDPV1Database::initialize()
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());

    if(!Connect())
        return false;

    do
    {
        if (!BeginTransaction())
        {
            rv = false;
            break;
        }

        if(!CreateRequiredTables())
        {
            rv = false;
            break;
        }

        if(!initializeVersion())
        {
            rv = false;
            break;
        }

        if(!initializeSuperBlock())
        {
            rv = false;
            break;
        }

        CDPVersion dbver;
        if(!read(dbver))
        {
            rv = false;
            break;
        }

        if( (dbver.c_revision != CDPVERCURRREV) && (!Upgrade(dbver.c_revision)))
        { 
            rv = false;
            break;
        }

        //(void)SetCacheSize(SV_DEFAULT_PAGES_IN_CDPDB_CACHE);

        if(!CommitTransaction())
        {
            rv = false;
            break;
        }

    } while(0);

    Disconnect();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1Database::SetCacheSize(SV_UINT numpages)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {

        stringstream action ;
        action << "PRAGMA default_cache_size=" 
            << numpages
            << " ;" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while setting cache size for database :" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n"
            << "Exception :" << (*m_con).errmsg() << "\n"
            << "Note: This error can be ignored.\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV1Database::initializeVersion()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());

    do
    {
        SVERROR hr_versiontblempty = isempty(CDPV1VERSIONTBLNAME);
        if(hr_versiontblempty.failed())
        {
            rv = false;
            break;
        }

        if(hr_versiontblempty  == SVS_OK)
        {		
            CDPVersion dbver;
            dbver.c_version = CDPVER;
            dbver.c_revision = CDPVERCURRREV;
            if(!InitializeVersionTable(dbver))
            {
                rv = false;
                break;
            }
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1Database::initializeSuperBlock()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());

    do
    {
        SVERROR hr_superblockempty = isempty(CDPV1SUPERBLOCKTBLNAME);
        if(hr_superblockempty.failed())
        {
            rv = false;
            break;
        }


        if(hr_superblockempty == SVS_OK)
        {
            CDPV1SuperBlock superblock;
            superblock.c_type    = CDP_UNDO;
            superblock.c_lookuppagesize = CDPUtil::LookupPageSize();
            superblock.c_physicalspace = 0;
            superblock.c_tsfc = 0;
            superblock.c_tslc = 0;
            superblock.c_numinodes = 0;
            superblock.c_eventsummary.clear();

            if(!InitializeSuperBlockTable(superblock))
            {
                rv = false;
                break;
            }
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1Database::getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end, SV_ULONGLONG & datafiles)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());


    if(!Connect())
        return false;

    do
    {
        CDPV1Database::Reader superBlockRdr = ReadSuperBlock();
        CDPV1SuperBlock superblock;
        if(superBlockRdr.read(superblock) != SVS_OK)
        {
            rv = false;
            break;
        }
        superBlockRdr.close();

        start = superblock.c_tsfc;
        end = superblock.c_tslc;
        datafiles = superblock.c_numinodes;

    } while(0);

    Disconnect();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1Database::get_cdp_inode(const std::string & filePath,
                                  bool& partiallyApplied,
                                  SV_USHORT recovery_accuracy,
                                  DiffPtr & change_metadata,
                                  //bool & alreadapplied,
                                  bool preallocate,
                                  CDPV1Inode & inode,
                                  bool isInDiffSync)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());

    if(!Connect())
        return false;

    do
    {
        SetResyncMode(!isInDiffSync);
        SetAccuracyMode(recovery_accuracy);

        if (!BeginTransaction())
        {
            rv = false;
            break;
        }

        //alreadapplied = false;

        ///
        /// if we have already applied the differential to log in last invocation
        /// we should not reapply
        ///

        //CDPV1AppliedDiffInfo appliedcheckentry, appliedresultentry;
        //appliedcheckentry.c_difffile = ACE::basename(filePath.c_str());
        //appliedcheckentry.c_startoffset = change_metadata -> m_start;
        //appliedcheckentry.c_endoffset = change_metadata -> m_end;

        //SVERROR hr_alreadyApplied = 
        //    FetchAppliedTableEntry(appliedcheckentry).read(appliedresultentry);
        //if ( hr_alreadyApplied.failed() )
        //{
        //    rv = false;
        //    break;
        //}

        //if (hr_alreadyApplied == SVS_OK)
        //{
        //    alreadapplied = true;
        //    rv = true;
        //    break;
        //}


        //Resync issues : If the inode list is empty the partiallyApplied should not be considered as the last transaction
        //might have got pruned.
        if (AllocateNewInodeIfRequired(change_metadata, preallocate, CDPV2DATAFILEPREFIX,partiallyApplied).failed())
        {
            rv = false;
            break;
        }

        CDPV1Database::Reader LatestInodeRdr = FetchLatestInode();
        SVERROR hr_currinode = LatestInodeRdr.read(inode,true);
        LatestInodeRdr.close();
        if ( hr_currinode != SVS_OK)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Internal Error while reading latest inode from CDP database " 
                << m_dbname << "\n";
            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());

            rv = false;
            break;
        }

        if(!CommitTransaction())
        {
            rv = false;
            break;
        }


    } while (0);

    Disconnect();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1Database::update_cdp(const std::string & filePath,
                               SV_USHORT recovery_accuracy,
                               DiffPtr & change_metadata,
                               DiffPtr & cdp_metadata,
                               CDPV1Inode & inode,
                               bool isInDiffSync)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());

    if(!Connect())
        return false;

    do
    {
        SetResyncMode(!isInDiffSync);
        SetAccuracyMode(recovery_accuracy);

        if (!BeginTransaction())
        {
            rv = false;
            break;
        }

        CDPV1SuperBlock superblock;
        CDPV1Database::Reader superBlockRdr = ReadSuperBlock();
        if(superBlockRdr.read(superblock) != SVS_OK)
        {
            rv = false;
            break;
        }
        superBlockRdr.close();

        SvMarkerPtr revokeTag;
        if(change_metadata ->GetRevocationTag(revokeTag))
        {
            SV_ULONGLONG inodeNo = 0;
            if(!FetchInodeNoForEvent(revokeTag ->TagType(), 
                revokeTag -> Tag().get(), 
                inodeNo))
            {
                rv = false;
                break;
            }

            if(inodeNo)
            {
                if(!RevokeTags(inodeNo))
                {
                    rv = false;
                    break;
                }
            }
        }


        //
        // ==================================================
        // superblock updates


        SV_ULONGLONG physicalsize = File::GetSizeOnDisk(inode.c_datadir + ACE_DIRECTORY_SEPARATOR_CHAR_A + inode.c_datafile);
        if(physicalsize)
        {
            //
            // PR # 6108
            // Following check/message was added on observing an incorrect message
            // related to retention space shortage in hosts.log
            // Example:
            // Warning: there is not enough free space left for
            // E:/log_F/9d58963592/C0129960-15CA-A341-AFE99E46BD4E4CAD/F to grow to its
            // specified capacity of 48318382080 bytes.Action: please free up at least
            // 8796093068229MB on the retention volume.
            // "8796093068229MB" is something wrong
            // 

            if(physicalsize >= inode.c_physicalsize)
            {
                superblock.c_physicalspace += ( physicalsize - inode.c_physicalsize);
            } 
            else
            {
                superblock.c_physicalspace -= ( inode.c_physicalsize - physicalsize);
                DebugPrintf(SV_LOG_WARNING, 
                    "physical size:" ULLSPEC " less than inode.c_physicalsize:" ULLSPEC "\n", 
                    physicalsize,
                    inode.c_physicalsize);
            }
        }

        if(!superblock.c_tsfc)
            superblock.c_tsfc = inode.c_starttime;
        superblock.c_tslc = cdp_metadata -> endtime();

        //
        // ==================================================================
        // inode updates           

        CDPV1DiffInfoPtr diffInfoPtr(new (nothrow) CDPV1DiffInfo);
        if(!diffInfoPtr)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Error: insufficient memory to allocate buffers\n";

            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        diffInfoPtr -> start= cdp_metadata -> StartOffset();
        diffInfoPtr -> end  = cdp_metadata -> EndOffset();
        diffInfoPtr -> tsfcseq = cdp_metadata -> StartTimeSequenceNumber();
        diffInfoPtr -> tsfc = cdp_metadata -> starttime();
        diffInfoPtr -> tslcseq = cdp_metadata -> EndTimeSequenceNumber();
        diffInfoPtr -> tslc = cdp_metadata -> endtime();
        diffInfoPtr -> ctid = cdp_metadata -> ContinuationId();
        diffInfoPtr -> numchanges = cdp_metadata -> NumDrtds();

        cdp_drtdv2s_iter_t drtdsIter = cdp_metadata -> DrtdsBegin();
        cdp_drtdv2s_iter_t drtdsEnd  = cdp_metadata -> DrtdsEnd();
        for( ; drtdsIter != drtdsEnd ; ++drtdsIter)
        {
            cdp_drtdv2_t drtd = *drtdsIter;
            diffInfoPtr -> changes.push_back(drtd);
        }


        inode.c_endseq = cdp_metadata -> EndTimeSequenceNumber();
        inode.c_endtime   = cdp_metadata -> endtime();
        inode.c_physicalsize = physicalsize;
        inode.c_logicalsize += cdp_metadata ->size();
        inode.c_numdiffs += 1;
        inode.c_diffinfos.push_back(diffInfoPtr);

        if(!cdp_metadata ->ContainsRevocationTag())
        {
            UserTagsIterator udtsIter = cdp_metadata  -> UserTagsBegin();
            UserTagsIterator udtsEnd  = cdp_metadata -> UserTagsEnd();
            for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
            {
                SvMarkerPtr pMarker = *udtsIter;
                SV_EVENT_TYPE EventType = pMarker -> TagType();
                superblock.c_eventsummary[EventType] +=1;
                inode.c_numevents += 1;
            }
        }

        if(!Update(inode))
        {
            rv = false;
            break;
        }

        //
        // ======================================================
        // update applied table

        //CDPV1AppliedDiffInfo appliedentry;
        //appliedentry.c_difffile = ACE::basename(filePath.c_str());
        //appliedentry.c_startoffset = change_metadata -> m_start;
        //appliedentry.c_endoffset = change_metadata -> m_end;

        //if(!Insert(appliedentry))
        //{
        //    rv = false;
        //    break;
        //}

        //
        // ======================================================
        // update time range and pending timerange tables


        CDPV1TimeRange timerange;
        CDPV1PendingTimeRange pendingtimerange;

        timerange.c_starttime = cdp_metadata -> starttime();
        timerange.c_endtime = cdp_metadata ->endtime();
        timerange.c_mode= recovery_accuracy;

        pendingtimerange.c_starttime =  cdp_metadata -> starttime();
        pendingtimerange.c_endtime = cdp_metadata ->endtime();
        pendingtimerange.c_mode = recovery_accuracy;

        // check if the corresponding inode does not have
        // per i/o information.
        // only then store the per i/o global sequence no
        // for the timerange
        if(inode.c_version >= CDPV1_INODE_PERIOVERSION)
        {
            timerange.c_startseq = cdp_metadata -> StartTimeSequenceNumber();
            timerange.c_endseq = cdp_metadata -> EndTimeSequenceNumber();
            pendingtimerange.c_startseq = cdp_metadata -> StartTimeSequenceNumber();
            pendingtimerange.c_endseq = cdp_metadata -> EndTimeSequenceNumber();
        } else
        {
            timerange.c_startseq = 0;
            timerange.c_endseq = 0;
            pendingtimerange.c_startseq = 0;
            pendingtimerange.c_endseq = 0;
        }

        if(!Add(timerange))
        {
            rv = false;
            break;        
        }

        if(!Add(pendingtimerange))
        {
            rv = false;
            break;        
        }

        //
        // ======================================================
        // update events

        DebugPrintf(SV_LOG_DEBUG, "Number of Tags: %d\n", cdp_metadata -> NumUserTags());

        if(!cdp_metadata ->ContainsRevocationTag())
        {

            UserTagsIterator udtsIter = cdp_metadata -> UserTagsBegin();
            UserTagsIterator udtsEnd  = cdp_metadata -> UserTagsEnd();
            for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
            {
                SvMarkerPtr pMarker = *udtsIter;
                CDPV1Event event;
                event.c_inodeno = inode.c_inodeno;
                event.c_eventtype = pMarker -> TagType();
                event.c_eventtime = cdp_metadata ->starttime();
                event.c_eventvalue = pMarker -> Tag().get();
                event.c_eventmode = recovery_accuracy;
                event.c_identifier = "";
                event.c_verified = NONVERIFIEDEVENT;
                event.c_comment ="";
                if(pMarker -> GuidBuffer())
                {
                    event.c_identifier = pMarker -> GuidBuffer().get();
                }
                DebugPrintf(SV_LOG_DEBUG,"getting tag: %s identifier: %s\n",event.c_eventvalue.c_str(),event.c_identifier.c_str());
                if(!Insert(event))
                {
                    rv = false;
                    break;
                }

                CDPV1PendingEvent pendingevent;
                pendingevent.c_inodeno = inode.c_inodeno;
                pendingevent.c_eventtype = pMarker -> TagType();
                pendingevent.c_eventtime = cdp_metadata -> starttime();
                pendingevent.c_eventvalue = pMarker -> Tag().get();
                pendingevent.c_valid   = true;
                pendingevent.c_eventmode = recovery_accuracy;
                pendingevent.c_identifier = "";
                pendingevent.c_verified = NONVERIFIEDEVENT;
                pendingevent.c_comment = "";

                if(pMarker -> GuidBuffer())
                {
                    pendingevent.c_identifier = pMarker -> GuidBuffer().get();
                }

                if(!Insert(pendingevent))
                {
                    rv = false;
                    break;
                }
            }

            if ( udtsIter != udtsEnd )
            {
                rv = false;
                break;
            }
        }

        //
        // ======================================================
        // update superblock

        if(!Update(superblock))
        {
            rv = false;
            break;
        }

        if(!CommitTransaction())
        {
            rv = false;
            break;
        }

    } while (0);

    Disconnect();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}


bool CDPV1Database::clear_applied_entry(const std::string & filePath)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_dbname.c_str());

    if(!Connect())
        return false;

    do
    {

        if (!BeginTransaction())
        {
            rv = false;
            break;
        }

        std::string fileName = basename_r(filePath.c_str());

        if(!DeleteAppliedEntries(fileName))
        {
            rv = false;
            break;
        }

        if(!CommitTransaction())
        {
            rv = false;
            break;
        }

    } while (0);

    Disconnect();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_dbname.c_str());
    return rv;
}

bool CDPV1Database::OlderVersionExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    string metadata = m_dbdir;
    metadata += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    metadata += "metadata";

    ACE_stat statbuf = {0};
    // PR#10815: Long Path support
    if ( 0 == sv_stat(getLongPathName(metadata.c_str()).c_str(), &statbuf) )
        rv = true ;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

///
/// Routines to verify existence of various table(s)
///

SVERROR CDPV1Database::SuperBlockExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1SUPERBLOCKTBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SVERROR CDPV1Database::InodeTableExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1INODETBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SVERROR CDPV1Database::EventTableExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1EVENTTBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



SVERROR CDPV1Database::TimeRangeTableExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1TIMERANGETBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



SVERROR CDPV1Database::PendingTimeRangeTableExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1PENDINGTIMERANGETBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SVERROR CDPV1Database::AppliedTableExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1APPLIEDTBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SVERROR  CDPV1Database::PendingEventTableExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1PENDINGEVENTTBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}
SVERROR  CDPV1Database::DataDirTableExists()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = TableExists(CDPV1DATADIRTBLNAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreateSuperBlockTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1SUPERBLOCKTBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_SUPERBLOCK_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_SUPERBLOCK_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating superblock for database :" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n"
            << "Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::InitializeSuperBlockTable(const CDPV1SuperBlock & superblock)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        EventSummary_t::const_iterator events_iter = superblock.c_eventsummary.begin();
        EventSummary_t::const_iterator events_end = superblock.c_eventsummary.end();

        stringstream eventsummary;
        for ( ; events_iter != events_end ; ++events_iter)
        {
            SV_EVENT_TYPE EventType = events_iter ->first;
            SV_ULONGLONG NumEvents = events_iter ->second;
            eventsummary << EventType << "," << NumEvents << ";" ;
        }


        stringstream action ;
        action << "insert into " << CDPV1SUPERBLOCKTBLNAME 
            << " ( c_type, c_lookuppagesize, c_physicalspace, " 
            << "   c_tsfc, c_tslc, c_numinodes, c_eventsummary )"
            << " values(" 
            << superblock.c_type << ","
            << superblock.c_lookuppagesize << ","
            << superblock.c_physicalspace << ","
            << superblock.c_tsfc << ","
            << superblock.c_tslc << ","
            << superblock.c_numinodes << ","
            << "'" <<  eventsummary.str() << "'"
            << ");" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while initializing superblock for database :" 
            << m_dbname << "\n" 
            << "Exception :" << ex.what() << "\n"
            << "Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreateInodeTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1INODETBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_INODE_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_INODE_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating inode table\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << "Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreateEventTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1EVENTTBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_EVENT_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_EVENT_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating event table\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << "Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreateTimeRangeTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1TIMERANGETBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_TIMERANGE_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_TIMERANGE_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating event table\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << "Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}




bool CDPV1Database::CreatePendingTimeRangeTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1PENDINGTIMERANGETBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_PENDINGTIMERANGE_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_PENDINGTIMERANGE_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating event table\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << "Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreateAppliedTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1APPLIEDTBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_APPLIED_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_APPLIED_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating applied table\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreatePendingEventTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1PENDINGEVENTTBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_PENDINGEVENT_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_PENDINGEVENT_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating pending event table\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreateDataDirTable()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream query ;
        query << "create table " << CDPV1DATADIRTBLNAME << "(" ;

        for (int i = 0; i < CDPV1_NUM_DATADIR_COLS ; ++i )
        {
            if(i)
                query << ",";
            query << CDPV1_DATADIR_COLS[i];
        }

        query << ");" ;

        (*m_con).executenonquery(query.str());
        rv = true;
    }
    catch (exception &ex)
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured while creating data dir table\n"
            << " Database : " << m_dbname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::InitializeDataDirs(list<string> & datadirs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        CDPV1DataDir  DirEntry;

        //// insert metadata directory
        //ZeroFill(DirEntry);
        //DirEntry.c_dir = m_dbdir;
        //if(!Insert(DirEntry))
        //{
        //	rv = false;
        //	break;
        //}
        //

        list<string>::iterator datadir_iter = datadirs.begin();
        list<string>::iterator datadir_end  = datadirs.end();

        for ( ; datadir_iter != datadir_end ; ++datadir_iter )
        {
            string dirname = *datadir_iter;	

            SVERROR hr_datadirname = this -> FetchDataDirForDirName(dirname).read(DirEntry);
            if(hr_datadirname.failed())
            {
                rv = false;
                break;
            }

            // the entry does not exist in database, so insert it
            if (hr_datadirname == SVS_FALSE)
            {
                ZeroFill(DirEntry);
                DirEntry.c_dir = dirname;
                if (!Insert(DirEntry))
                {
                    rv = false;
                    break;
                }
            }
        }

        if ( datadir_iter != datadir_end )
        {
            rv = false;
            break;
        }

        rv = true;
    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::CreateRequiredTables()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do 
    {
        SVERROR hr_versiontblexists = VersionTblExists();
        if(hr_versiontblexists.failed())
        {
            rv = false;
            break;
        }

        if(hr_versiontblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreateVersionTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }

        SVERROR hr_superblockexists = SuperBlockExists();
        if ( hr_superblockexists.failed())
        {
            rv = false;
            break;
        }

        if(hr_superblockexists == SVS_FALSE) 
        {
            bool rv_tmp = CreateSuperBlockTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }

        SVERROR hr_datadirtblexists = DataDirTableExists();
        if ( hr_datadirtblexists.failed())
        {
            rv = false;
            break;
        }

        if(hr_datadirtblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreateDataDirTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }

        SVERROR hr_inodetblexists = InodeTableExists();
        if ( hr_inodetblexists.failed())
        {
            rv = false;
            break;
        }

        if(hr_inodetblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreateInodeTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }

        SVERROR hr_eventtblexists = EventTableExists();
        if ( hr_eventtblexists.failed() )
        {
            rv = false;
            break;
        }

        if(hr_eventtblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreateEventTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }

        SVERROR hr_pendingeventtblexists = PendingEventTableExists();
        if ( hr_pendingeventtblexists.failed() )
        {
            rv = false;
            break;
        }

        if(hr_pendingeventtblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreatePendingEventTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }

        SVERROR hr_appliedtblexists = AppliedTableExists();
        if ( hr_appliedtblexists.failed() )
        {
            rv = false;
            break;
        }


        if(hr_appliedtblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreateAppliedTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }


        SVERROR hr_classtblexists = TimeRangeTableExists();
        if ( hr_classtblexists.failed() )
        {
            rv = false;
            break;
        }

        if(hr_classtblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreateTimeRangeTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }


        SVERROR hr_pendingtblexists = PendingTimeRangeTableExists();
        if ( hr_pendingtblexists.failed() )
        {
            rv = false;
            break;
        }

        if(hr_pendingtblexists == SVS_FALSE) 
        {
            bool rv_tmp = CreatePendingTimeRangeTable();
            if(!rv_tmp)
            {
                rv = false;
                break;
            }
        }

        rv = true;
    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::GetRevision(double & rev)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    CDPVersion ver;
    if(read(ver))
    {
        if( ver.c_version == CDPVER1 )
        {
            rev = ver.c_revision;
            rv = true;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPV1Database::Upgrade
*
* DESCRIPTION : 
*
*   make modification to tables for 
*   upgrading to current(new) revision
*
* INPUT PARAMETERS : 
*
*  oldversion - version of existing database
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::Upgrade(double oldversion) 
{
    bool rv = false;

    if(oldversion == CDPVER1REV0 ) {
        rv = UpgradeRev0();
    } else if(oldversion == CDPVER1REV1 ) {
        rv = UpgradeRev1();
    } else if (oldversion == CDPVER1REV2) {
        rv = UpgradeRev2();
    } else {
        rv = false;
    }

    return rv;
}

bool CDPV1Database::UpgradeRev2()
{
    bool rv = false;
    stringstream query ;

    do
    {
        if(!UpgradeRev1EventTable())
        {
            rv = false;
            break;
        }

        if(!UpgradeRev1PendingEventTable())
        {
            rv = false;
            break;
        }
        CDPVersion ver;
        ver.c_version=CDPVER;
        ver.c_revision=CDPVERCURRREV;

        if(!write(ver))
        {
            rv=false;
            break;
        }

        rv = true;

    } while (0);

    return rv;
}
/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev1
*
* DESCRIPTION : 
*
*   upgrade from revision 1 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*  current revision: 2.0
*  changes introduced in revision : 2.0
*    1) version column was added to inode table
*    2) startseq and endseq column were added to timerange table
*    3) startseq and endseq column were added to pendingtimerange table
*
* return value : true on success, false otherwise
*
*/

bool CDPV1Database::UpgradeRev1()
{
    bool rv = false;
    stringstream query ;

    do
    {
        if(!UpgradeRev1InodeTable())
        {
            rv = false;
            break;
        }

        if(!UpgradeRev1TimeRangeTable())
        {
            rv = false;
            break;
        }

        if(!UpgradeRev1PendingTimeRangeTable())
        {
            rv = false;
            break;
        }

        if(!UpgradeRev1EventTable())
        {
            rv = false;
            break;
        }

        if(!UpgradeRev1PendingEventTable())
        {
            rv = false;
            break;
        }
        CDPVersion ver;
        ver.c_version=CDPVER;
        ver.c_revision=CDPVERCURRREV;

        if(!write(ver))
        {
            rv=false;
            break;
        }

        rv = true;

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev1InodeTable
*
* DESCRIPTION : 
*
*   upgrade inode table from revision 1 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*  current revision: 2.0
*  changes introduced in revision : 2.0
*    1) version column was added to inode table
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::UpgradeRev1InodeTable()
{
    bool rv = false;
    stringstream query ;
    try 
    {

        query << " ALTER TABLE " << CDPV1INODETBLNAME << " ADD COLUMN " 
            << " c_version INTEGER DEFAULT " << CDPV1_INODE_VERSION0 ;

        (*m_con).executenonquery(query.str());
        rv=true;
    }
    catch (exception &ex)	
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1INODETBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;              
    }

    return rv;
}


/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev1TimeRangeTable
*
* DESCRIPTION : 
*
*   upgrade timerange table from revision 1 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*  current revision: 2.0
*  changes introduced in revision : 2.0
*    1) c_startseq and c_endseq column was added to timerange table
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::UpgradeRev1TimeRangeTable()
{
    bool rv = false;
    stringstream query ;
    try 
    {

        query << " ALTER TABLE " << CDPV1TIMERANGETBLNAME << " ADD COLUMN " 
            << " c_startseq INTEGER DEFAULT 0 " ;

        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1TIMERANGETBLNAME << " ADD COLUMN "
            << " c_endseq INTEGER DEFAULT 0 ";

        (*m_con).executenonquery(query.str());

        rv=true;
    }
    catch (exception &ex)	
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1TIMERANGETBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;              
    }

    return rv;
}


/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev1PendingTimeRangeTable
*
* DESCRIPTION : 
*
*   upgrade timerange table from revision 1 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*  current revision: 2.0
*  changes introduced in revision : 2.0
*    1) c_startseq and c_endseq column was added to pendingtimerange table
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::UpgradeRev1PendingTimeRangeTable()
{
    bool rv = false;
    stringstream query ;
    try 
    {
        query << " ALTER TABLE " << CDPV1PENDINGTIMERANGETBLNAME << " ADD COLUMN "
            << " c_startseq INTEGER DEFAULT 0 " ;

        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1PENDINGTIMERANGETBLNAME << " ADD COLUMN "
            << " c_endseq INTEGER DEFAULT 0 ";

        (*m_con).executenonquery(query.str());

        rv=true;
    }
    catch (exception &ex)	
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1PENDINGTIMERANGETBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;              
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev1EventTable
*
* DESCRIPTION : 
*
*   upgrade timerange table from revision 1 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*  current revision: 2.0
*  changes introduced in revision : 2.0
*    1) c_identifier column was added to event table
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::UpgradeRev1EventTable()
{
    bool rv = false;
    stringstream query ;
    try 
    {

        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";

        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        (*m_con).executenonquery(query.str());

        rv=true;
    }
    catch (exception &ex)	
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1EVENTTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;              
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev1PendingEventTable
*
* DESCRIPTION : 
*
*   upgrade timerange table from revision 1 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*  current revision: 2.0
*  changes introduced in revision : 2.0
*    1) c_identifier column was added to pending event table
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::UpgradeRev1PendingEventTable()
{
    bool rv = false;
    stringstream query ;
    try 
    {

        query << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";

        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        (*m_con).executenonquery(query.str());

        rv=true;
    }
    catch (exception &ex)	
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1EVENTTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;              
    }

    return rv;
}


/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev0InodeTable
*
* DESCRIPTION : 
*
*   upgrade inode table from revision 0 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*  current revision: 2.0
*  changes introduced in revision : 2.0
*    1) version column was added to inode table
*
* there is no change to inode table between revision 0 to
* revision 1. so, just call UpgradeRev1InodeTable()
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::UpgradeRev0InodeTable()
{
    return UpgradeRev1InodeTable();
}


/*
* FUNCTION NAME :  CDPV1Database::UpgradeRev0
*
* DESCRIPTION : 
*
*   upgrade from revision 0 to current revision 
*  
*
* INPUT PARAMETERS : none
*
*                   
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*  changes introduced in revision : 1.0
*    1) eventmode column was added to event table
*    2) event mode column was added to pending event table
*    3) timerange table was newly added
*    4) pendingtimerange table newly added
*
*  changes introduced in revision : 2.0
*    1) version column was added to inode table
*
* return value : true on success, false otherwise
*
*/
bool CDPV1Database::UpgradeRev0()
{	
    bool rv = true;
    stringstream query ;
    CDPV1SuperBlock superblock;

    do{

        if(!UpgradeRev0EventTable())
        {
            rv=false;
            break;
        }
        if(!UpgradeRev0PendingEventTable())
        {
            rv=false;
            break;
        }

        Reader superblockRdr = ReadSuperBlock();
        if(superblockRdr.read(superblock) != SVS_OK)
        {
            rv = false;
            break;
        }

        superblockRdr.close();

        if(!UpgradeRev0TimeRangeTable(superblock))
        {
            rv = false;
            break;
        }

        if(!UpgradeRev0PendingTimeRangeTable(superblock))
        {
            rv = false;
            break;
        }

        if(!UpgradeRev0InodeTable())
        {
            rv=false;
            break;
        }

        CDPVersion ver;
        ver.c_version=CDPVER;
        ver.c_revision=CDPVERCURRREV;

        if(!write(ver))
        {
            rv=false;
            break;
        }

        rv = true;
    }while(0);       

    return rv;
}

bool CDPV1Database::UpgradeRev0TimeRangeTable(CDPV1SuperBlock & superblock)
{
    bool rv = true;

    do
    {
        if(!superblock.c_tsfc || !superblock.c_tslc)
        {
            rv = true;
            break;
        }

        //check timerange table for empty
        //if empty then Initialise it         
        SVERROR hr_timerangetableempty = isempty(CDPV1TIMERANGETBLNAME);
        if(hr_timerangetableempty.failed())
        {
            rv = false;
            break;
        }

        if(hr_timerangetableempty == SVS_OK)
        {
            CDPV1TimeRange timerange;
            timerange.c_starttime = superblock.c_tsfc;
            timerange.c_endtime = superblock.c_tslc;
            timerange.c_mode = 1;

            if(!Insert(timerange))
            {
                rv=false;
                break;
            }
        }

        rv = true;
    } while(0);

    return rv;
}

bool CDPV1Database::UpgradeRev0PendingTimeRangeTable(CDPV1SuperBlock & superblock)
{
    bool rv = true;

    do
    {
        if(!superblock.c_tsfc || !superblock.c_tslc)
        {
            rv = true;
            break;
        }

        //check pending timerange table for empty
        //if empty then Initialise it         
        SVERROR hr_pendingtimerangetableempty = isempty(CDPV1PENDINGTIMERANGETBLNAME);
        if(hr_pendingtimerangetableempty.failed())
        {
            rv = false;
            break;
        }

        if(hr_pendingtimerangetableempty == SVS_OK)
        {
            CDPV1PendingTimeRange pendingtimerange;
            pendingtimerange.c_starttime = superblock.c_tsfc;
            pendingtimerange.c_endtime = superblock.c_tslc;
            pendingtimerange.c_mode = 1;

            if(!Insert(pendingtimerange))
            {
                rv=false;
                break;
            }
        } 

        rv = true;
    } while (0);

    return rv;
}

bool CDPV1Database::UpgradeRev0EventTable()
{
    bool rv = false;
    stringstream query ;
    try {

        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_eventmode INTEGER DEFAULT 1 " ;

        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";

        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        (*m_con).executenonquery(query.str());

        rv=true;
    }catch (exception &ex)	{
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1EVENTTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;              
    }
    return rv;


}
bool CDPV1Database::UpgradeRev0PendingEventTable()
{
    bool rv = false;
    stringstream query ;
    try {            
        query << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_eventmode INTEGER DEFAULT 1 " ;
        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1PENDINGEVENTTBLNAME << " ADD COLUMN " 
            << " c_identifier BLOB DEFAULT NULL";

        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_verified INTEGER DEFAULT 0";
        (*m_con).executenonquery(query.str());

        query.str("");
        query << " ALTER TABLE " << CDPV1EVENTTBLNAME << " ADD COLUMN " 
            << " c_comment BLOB DEFAULT NULL";
        (*m_con).executenonquery(query.str());

        rv=true;
    }catch (exception &ex)	{
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n\n"
            << "Exception Occured while upgrading database version :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << CDPV1PENDINGEVENTTBLNAME << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;         
    }
    return rv;

}

SVERROR CDPV1Database::AllocateNewInodeIfRequired(DiffPtr & diffptr,
                                                  bool preAllocate,
                                                  const std::string& cdpDataFileVerPrefix, bool& partiallyApplied)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVS_FALSE;

    do
    {	
        CDPV1Inode cdplatestinode;
        SVERROR hr_newinodeRequired = SVS_FALSE;
        CDPV1Database::Reader latestInodeRdr = FetchLatestInode();
        SVERROR hr_latestinode = latestInodeRdr.read(cdplatestinode,false);
        latestInodeRdr.close();
        if (hr_latestinode.failed())
        {
            hr = SVE_FAIL;
            break;
        }

        if (hr_latestinode == SVS_FALSE)
        {
            cdplatestinode.c_inodeno = -1;
            hr_newinodeRequired = SVS_OK;
            if(partiallyApplied)
            {
                partiallyApplied = false;
                DebugPrintf(SV_LOG_DEBUG,"AllocateNewInodeIfRequired - Partially Applied is true but Inode list empty\n");
            }
        }

        if (hr_latestinode == SVS_OK)
        {
            if(!partiallyApplied)
            {
                hr_newinodeRequired =  NewInodeRequired(diffptr, cdplatestinode, cdpDataFileVerPrefix);
                if( hr_newinodeRequired.failed() )
                {
                    hr = SVE_FAIL;
                    break;
                }

                if(hr_newinodeRequired == SVS_FALSE)
                {
                    hr = SVS_FALSE;
                    break;
                }
            }
            else
            {
                hr = SVS_FALSE;
                break;
            }
        }

        if(hr_newinodeRequired == SVS_OK) 
        {
            if(!CreateNewInode(diffptr, cdplatestinode, preAllocate, cdpDataFileVerPrefix))
            {
                hr = SVE_FAIL;
                break;
            }
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

SVERROR CDPV1Database::NewInodeRequired(const DiffPtr & diffptr,
                                        const CDPV1Inode & cdpcurrinode,
                                        const std::string& cdpDataFileVerPrefix)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVS_FALSE;

    do
    {

        // Every resync data goes into a seperate data file
        if(isResyncMode())
        {
            hr = SVS_OK;
            break;
        }

        // each inode either stores resync data/split io data/non split io
        // it cannot store a combination of those
        if(cdpcurrinode.c_type != diffptr ->type())
        {
            hr = SVS_OK;
            break;
        }

        // 
        // PR # 6362
        // if the inode contains data of older format (ie before upgrade)
        // we will create a new inode, the current inode cannot
        // hold data for the incoming differential of newer format

        //
        // windows agent still send data in old format
        // so we continue to store data in old format (ie no per i/o info) for windows source case
        // create new inode only if incoming diff has per i/o information and previous inode does not
        // support it
        if( (cdpcurrinode.c_version < CDPV1_INODE_PERIOVERSION) && (diffptr ->version() >= SVD_PERIOVERSION) )
        {
            hr = SVS_OK;
            break;
        }


        // create a new file if the previous one was resync and this is diff sync
        // note: this check may be redundant
        std::string::size_type isresyncfile = cdpcurrinode.c_datafile.find(CDPRESYNCDATAFILE);
        if ((std::string::npos != isresyncfile) && !isResyncMode())
        {
            hr = SVS_OK;
            break;
        }

        // create a new file if the data file format has changed
        // new format has prefix cdpv2, old format has prefix cdpv1
        if(0 != strncmp(cdpcurrinode.c_datafile.c_str(),
            cdpDataFileVerPrefix.c_str(),
            cdpDataFileVerPrefix.length()))
        {
            hr = SVS_OK;
            break;
        }

        // if current inode is empty (stale inode), new inode should not be created
        if( cdpcurrinode.c_logicalsize == 0 )
        {
            hr = SVS_FALSE;
            break;
        }

        SV_ULONGLONG starttime = cdpcurrinode.c_starttime;
        SV_ULONGLONG endtime   = 0;
        (isResyncMode())?(endtime = cdpcurrinode.c_endtime):(endtime = diffptr -> endtime());

        if ( 
            ( diffptr -> ContainsMarker() )
            || ( (cdpcurrinode.c_logicalsize + diffptr -> size()) >= CDPUtil::MaxSpacePerDataFile())
            || ( (endtime -  starttime) >= CDPUtil::MaxTimeRangePerDataFile())
            )
        {
            hr = SVS_OK;
            break;
        }

        hr = SVS_FALSE;

    } while ( FALSE );

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr ;
}

bool CDPV1Database::CreateNewInode(const DiffPtr & diffptr,
                                   const CDPV1Inode & cdpcurrinode,
                                   bool preAllocate,
                                   const std::string& cdpDataFileVerPrefix)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        CDPV1Inode cdpinode;

        cdpinode.c_datafile  = NewDataFileName(diffptr, cdpDataFileVerPrefix);
        vector<CDPV1DataDir> datadirs;

        CDPV1Database::Reader dataDirsRdr = FetchDataDirs();
        if(!dataDirsRdr.read(datadirs))
        {
            rv = false;
            break;
        }
        dataDirsRdr.close();

        vector<CDPV1DataDir>::iterator datadirsIter = datadirs.begin();
        vector<CDPV1DataDir>::iterator datadirsEnd = datadirs.end();

        if (datadirsIter == datadirsEnd)
        {
            SV_ULONGLONG freespace;
            if(!GetDiskFreeCapacity(m_dbdir, freespace))
            {
                rv = false;
                break;
            }

            if( freespace < diffptr -> size())
            {
                stringstream l_stderr;
                l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << " Error: insufficient space under " << m_dbdir 
                    << "to accomadate incoming differential file\n"
                    << "Available Free Space:" << freespace << "\n"
                    << "Required Free Space:"  << diffptr -> size() << "\n"
                    << " Database : " << m_dbname << "\n"
                    << "Differentials will not be applied until the log capacity is increased"
                    << "Action: Increase the log capacity and create the appropriate free space on "
                    << "retention log directory\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

                rv = false;
                break;
            }

            cdpinode.c_datadir = m_dbdir;
            cdpinode.c_dirnum = 0;
        }
        else
        {
            for ( ; datadirsIter != datadirsEnd; ++datadirsIter)
            {
                CDPV1DataDir datadir = *datadirsIter;
                SV_ULONGLONG freespace;
                if(!GetDiskFreeCapacity(datadir.c_dir, freespace))
                {
                    rv = false;
                    break;
                }

                if( freespace >= diffptr -> size())
                {
                    cdpinode.c_datadir = datadir.c_dir;
                    cdpinode.c_dirnum = datadir.c_dirnum;
                    break;
                }
            }

            if( datadirsIter == datadirsEnd )
            {
                stringstream l_stderr;
                l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << " Error: insufficient space to accomodate incoming differential file\n"
                    << "Required Free Space:"  << diffptr -> size() << "\n"
                    << " Database : " << m_dbname << "\n"
                    << "Differentials will not be applied until the log capacity is increased"
                    << "Action: Increase the log capacity and create the appropriate free space on "
                    << "retention log directory\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

                rv = false;
                break;
            }
        }


        if(isResyncMode())
        {
            cdpinode.c_type = RESYNC_MODE; 
            cdpinode.c_startseq  = cdpcurrinode.c_endseq;
            cdpinode.c_starttime = cdpcurrinode.c_endtime;
            cdpinode.c_endseq  = cdpcurrinode.c_endseq;
            cdpinode.c_endtime = cdpcurrinode.c_endtime;
        }
        else
        {
            cdpinode.c_type = diffptr -> type();
            cdpinode.c_startseq =  diffptr -> StartTimeSequenceNumber();
            cdpinode.c_starttime = diffptr -> starttime();
            cdpinode.c_endseq =  diffptr -> StartTimeSequenceNumber();
            cdpinode.c_endtime = diffptr -> starttime();
        }

        cdpinode.c_logicalsize  = 0;
        cdpinode.c_physicalsize = 0;
        cdpinode.c_numdiffs     = 0;
        cdpinode.c_numevents    = 0;

        // PR #6362
        // if the incoming diff is of new format
        // the inode will contain per i/o information
        if(diffptr ->version() >= SVD_PERIOVERSION)
        {
            cdpinode.c_version = CDPV1_INODE_CURVER;
        } 
        else  
        {
            if(cdpcurrinode.c_inodeno == -1)
            {
                // if this is first inode and the incoming file
                // is either a tso file or older format
                // inode will not contain per i/o information
                // but it does contain type info (split i/o or non split i/o)
                cdpinode.c_version = CDPV1_INODE_VERSION0;
            } 
            else
            {
                // 
                // we do not parse tso files, so we do not
                // know if it is per i/o diff file or older format
                // so we will use the version of latest inode
                // the assumption is once we start getting a 
                // new format file, we cannot get older format later
                // on
                cdpinode.c_version = cdpcurrinode.c_version;
            }
        }

        if (!Insert(cdpinode) )
        {
            rv = false;
            break;
        }

        CDPV1SuperBlock superblock;
        CDPV1Database::Reader superBlockRdr = ReadSuperBlock();
        SVERROR hr_superblock = superBlockRdr.read(superblock);
        superBlockRdr.close();
        if (hr_superblock != SVS_OK)
        {
            rv = false;
            break;
        }


        superblock.c_numinodes += 1;

        if(!Update(superblock))
        {
            rv = false;
            break;
        }

        File datafile(cdpinode.c_datadir + ACE_DIRECTORY_SEPARATOR_CHAR_A + cdpinode.c_datafile);
        datafile.Open(BasicIo::BioRWAlways |  BasicIo::BioShareRead | BasicIo::BioBinary);
        if ( !datafile.Good() )
        {
            stringstream l_stderr;
            l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Error while creating " << datafile.GetName() << "\n"
                << "Error Code: " << datafile.LastError() << "\n"
                << "Error Message: " << Error::Msg(datafile.LastError()) << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n";
            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        if(preAllocate)
        {
            SV_ULONGLONG MaxRetentionFileSz = CDPUtil::MaxSpacePerDataFile();
            datafile.PreAllocate(MaxRetentionFileSz);
        }
        datafile.Close();

        rv = true;
    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv ;
}

string CDPV1Database::NewDataFileName(const DiffPtr & diffptr, const std::string& cdpDataFileVerPrefix)
{
    stringstream name;
    string fileprefix;
    static std::string const DatExtension(".dat");

    if (isResyncMode())
        fileprefix = CDPRESYNCDATAFILE;
    else
    {
        fileprefix = CDPDIFFSYNCDATAFILE;
        if(diffptr -> type() == SPLIT_IO)
        {
            fileprefix += CDPSPLITIOFILE;
        } else if(diffptr -> type() == OUTOFORDER_TS)
        {
            fileprefix += CDPOUTOFORDERTSFILE;
        } else if(diffptr -> type() == OUTOFORDER_SEQ)
        {
            fileprefix += CDPOUTOFORDERSEQFILE;
        } else
        {
            fileprefix += CDPNONSPLITIOFILE;
        } 
    }

    ACE_stat db_stat ={0};
    std::string fullDataFileName;
    while(true)
    {
        name.str("");
        ACE_Time_Value aceTv = ACE_OS::gettimeofday();         
        name << cdpDataFileVerPrefix << "_" << fileprefix  << "_" 
            << std::hex << std::setw(16) << std::setfill('0') 
            << ACE_TIME_VALUE_AS_USEC(aceTv) << DatExtension;

        fullDataFileName = m_dbdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + name.str();
        // PR#10815: Long Path support
        if(sv_stat(getLongPathName(fullDataFileName.c_str()).c_str(),&db_stat) != 0)
        {
            break;
        }
        DebugPrintf(SV_LOG_DEBUG,
            "CDP data file with name %s already exists. regenerating new name.\n",
            fullDataFileName.c_str());
        ACE_OS::sleep(1);
    }

    return name.str();
}


bool CDPV1Database::Insert(const DiffPtr & diffptr, CDPV1Inode & inode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        CDPV1SuperBlock superblock;
        CDPV1Database::Reader superBlockRdr = ReadSuperBlock();
        SVERROR hr_superblock = superBlockRdr.read(superblock);
        superBlockRdr.close();
        if (hr_superblock != SVS_OK)
        {
            rv = false;
            break;
        }

        bool bRevocationTag = diffptr ->ContainsRevocationTag();

        inode.c_endseq = diffptr -> EndTimeSequenceNumber();
        inode.c_endtime   = diffptr -> endtime();

        SV_ULONGLONG physicalsize = File::GetSizeOnDisk(inode.c_datadir + ACE_DIRECTORY_SEPARATOR_CHAR_A + inode.c_datafile);
        if(physicalsize)
        {
            //
            // PR # 6108
            // Following check/message was added on observing an incorrect message
            // related to retention space shortage in hosts.log
            // Example:
            // Warning: there is not enough free space left for
            // E:/log_F/9d58963592/C0129960-15CA-A341-AFE99E46BD4E4CAD/F to grow to its
            // specified capacity of 48318382080 bytes.Action: please free up at least
            // 8796093068229MB on the retention volume.
            // "8796093068229MB" is something wrong
            // 

            if(physicalsize >= inode.c_physicalsize)
            {
                superblock.c_physicalspace += ( physicalsize - inode.c_physicalsize);
            } else
            {
                DebugPrintf(SV_LOG_ERROR, "physical size:" ULLSPEC " less than inode.c_physicalsize:" ULLSPEC "\n", physicalsize, inode.c_physicalsize);
            }
        }

        if(!superblock.c_tsfc)
            superblock.c_tsfc = inode.c_starttime;

        superblock.c_tslc = diffptr -> endtime();

        if(!bRevocationTag)
        {
            UserTagsIterator udtsIter = diffptr -> UserTagsBegin();
            UserTagsIterator udtsEnd  = diffptr -> UserTagsEnd();
            for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
            {
                SvMarkerPtr pMarker = *udtsIter;
                SV_EVENT_TYPE EventType = pMarker -> TagType();
                superblock.c_eventsummary[EventType] +=1;
            }
        }

        inode.c_endtime = diffptr -> endtime();
        inode.c_physicalsize = physicalsize;
        inode.c_logicalsize += diffptr ->size();
        inode.c_numdiffs += 1;

        if(!bRevocationTag)
            inode.c_numevents += diffptr -> NumUserTags();

        CDPV1DiffInfoPtr diffInfoPtr(new (nothrow) CDPV1DiffInfo);
        if(!diffInfoPtr)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Error: insufficient memory to allocate buffers\n";

            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        diffInfoPtr -> start= diffptr -> StartOffset();
        diffInfoPtr -> end  = diffptr -> EndOffset();
        diffInfoPtr -> tsfcseq = diffptr -> StartTimeSequenceNumber();
        diffInfoPtr -> tsfc = diffptr -> starttime();
        diffInfoPtr -> tslcseq = diffptr -> EndTimeSequenceNumber();
        diffInfoPtr -> tslc = diffptr -> endtime();
        diffInfoPtr -> ctid = diffptr -> ContinuationId();
        diffInfoPtr -> numchanges = diffptr -> NumDrtds();

        cdp_drtdv2s_iter_t drtdsIter = diffptr -> DrtdsBegin();
        cdp_drtdv2s_iter_t drtdsEnd  = diffptr -> DrtdsEnd();
        for( ; drtdsIter != drtdsEnd ; ++drtdsIter)
        {
            cdp_drtdv2_t drtd = *drtdsIter;
            diffInfoPtr -> changes.push_back(drtd);
        }

        inode.c_diffinfos.push_back(diffInfoPtr);


        if(!Update(inode))
        {
            rv = false;
            break;
        }
        if(!Update(superblock))
        {
            rv = false;
            break;
        }

        if(!bRevocationTag)
        {
            if(!AddEvents(inode,diffptr))
            {
                rv = false;
                break;
            }
        }

        rv = true;
    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::AddEvents(const CDPV1Inode & cdpinode, const DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        UserTagsIterator udtsIter = diffptr -> UserTagsBegin();
        UserTagsIterator udtsEnd  = diffptr -> UserTagsEnd();
        for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
        {
            SvMarkerPtr pMarker = *udtsIter;
            CDPV1Event event;
            event.c_inodeno = cdpinode.c_inodeno;
            event.c_eventtype = pMarker -> TagType();
            event.c_eventtime = diffptr ->starttime();
            event.c_eventvalue = pMarker -> Tag().get();
            event.c_eventmode = AccuracyMode();
            event.c_identifier = "";
            event.c_verified = NONVERIFIEDEVENT;
            event.c_comment ="";
            if(pMarker -> GuidBuffer())
            {
                event.c_identifier = pMarker ->GuidBuffer().get();
            }			

            if(!Insert(event))
            {
                rv = false;
                break;
            }

            CDPV1PendingEvent pendingevent;
            pendingevent.c_inodeno = cdpinode.c_inodeno;
            pendingevent.c_eventtype = pMarker -> TagType();
            pendingevent.c_eventtime = diffptr -> starttime();
            pendingevent.c_eventvalue = pMarker -> Tag().get();
            pendingevent.c_valid   = true;
            pendingevent.c_eventmode = AccuracyMode();
            pendingevent.c_identifier = "";
            pendingevent.c_verified = NONVERIFIEDEVENT;
            pendingevent.c_comment = "";
            if(pMarker -> GuidBuffer())
            {
                pendingevent.c_identifier = pMarker ->GuidBuffer().get();
            }

            if(!Insert(pendingevent))
            {
                rv = false;
                break;
            }
        }

        if ( udtsIter == udtsEnd )
        {
            rv = true;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

CDPV1Database::Reader CDPV1Database::ReadSuperBlock()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1SUPERBLOCKTBLNAME;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchDataDirs()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1DATADIRTBLNAME << " order by c_dirnum";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchDataDirForDirNum(SV_LONG dirnum)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1DATADIRTBLNAME << " where c_dirnum=" << dirnum ;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchDataDirForDirName(const string & dirname)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    // PR#10815: Long Path support
    char cdirname[SV_MAX_PATH];
    inm_strcpy_s(cdirname, ARRAYSIZE(cdirname), dirname.c_str());
    PathRemoveAllBackslashes(cdirname);

    stringstream query ;
    query << "select * from " << CDPV1DATADIRTBLNAME << " where c_dir='" << cdirname << "'";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchInodesinOldestFirstOrder()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1INODETBLNAME << " order by c_inodeno ASC";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}


CDPV1Database::Reader CDPV1Database::FetchInodesinLatestFirstOrder()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1INODETBLNAME << " order by c_inodeno DESC";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchOldestInode()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1INODETBLNAME << " order by c_inodeno ASC limit 1";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchLatestInode()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1INODETBLNAME << " order by c_inodeno DESC limit 1";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchLatestDiffSyncInode()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1INODETBLNAME 
        << " where c_type <> 0"
        << " order by c_inodeno DESC limit 1";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchInode(const SV_ULONGLONG & inodeNo)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1INODETBLNAME 
        << " where c_inodeno=" << inodeNo ;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchInodeWithEndTimeGrEq(const SV_ULONGLONG & ts)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1INODETBLNAME 
        << " where c_endtime >= " 
        <<  ts 
        << " order by c_inodeno ASC limit 1" ;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());

}

bool CDPV1Database::FetchInodeNoForEvent(const std::string & eventvalue, const SV_ULONGLONG & eventtime, SV_ULONGLONG & inodeNo ) 
{ 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME); 
    bool rv = false; 

    do { 
        try 
        { 
            stringstream query ; 
            query << "select c_inodeno from " << CDPV1EVENTTBLNAME 
                << " where c_eventvalue=" << '?' 
                << " AND c_eventtime=" << eventtime ; 

            boost::shared_ptr<sqlite3_command> cmd(new (nothrow) sqlite3_command(*m_con, query.str()));
            if(!cmd) 
            { 
                // you will never reach here due to exception 
                rv = false; 
                break; 
            } 

            cmd -> bind( 1, eventvalue ); 
            sqlite3_reader rdr = cmd -> executereader(); 
            if(!rdr.read()) 
            { 
                inodeNo = 0;
                rv = false; 
                break; 
            } 

            inodeNo = rdr.getint64(0); 
            rv = true; 
        } 
        catch(exception &ex) 
        { 
            stringstream l_stderr; 
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

bool CDPV1Database::FetchInodeNoForEvent(SV_USHORT eventType, const std::string & eventvalue, SV_ULONGLONG & inodeNo)
{ 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME); 
    bool rv = false; 

    do { 
        try 
        { 
            stringstream query ; 
            query << "select c_inodeno from " << CDPV1EVENTTBLNAME 
                << " where c_eventvalue=" << '?' 
                << " AND c_eventtype=" << eventType ; 

            boost::shared_ptr<sqlite3_command> cmd(new (nothrow) sqlite3_command(*m_con, query.str()));
            if(!cmd)

            { 
                // you will never reach here due to exception 
                rv = false; 
                break; 
            } 

            cmd -> bind( 1, eventvalue ); 
            sqlite3_reader rdr = cmd -> executereader(); 
            if(!rdr.read()) 
            { 
                inodeNo = 0;
                rv = true; 
                break; 
            } 

            inodeNo = rdr.getint64(0); 
            rv = true; 
        } 
        catch(exception &ex) 
        { 
            stringstream l_stderr; 
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n" 
                << "Exception Occured during read operation\n" 
                << " Database : " << m_dbname << "\n" 
                << " Exception :"  << ex.what() << "\n" 
                << USER_DEFAULT_ACTION_MSG << "\n" ; 

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str()); 
            rv = false; 
        } 
    } while (0); 

    return rv; 
} 
CDPV1Database::Reader CDPV1Database::FetchEventsForInode(const CDPV1Inode & inode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1EVENTTBLNAME 
        << " where c_inodeno=" << inode.c_inodeno
        << " order by c_eventno " << " ASC " << " limit " << inode.c_numevents;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchEventsForInode(const SV_ULONGLONG & inodeNo )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1EVENTTBLNAME 
        << " where c_inodeno=" << inodeNo;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchEvents()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1EVENTTBLNAME << " order by c_eventno";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchPendingEvents()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1PENDINGEVENTTBLNAME << " order by c_eventno";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchTimeRanges()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1TIMERANGETBLNAME << " order by c_entryno DESC";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchPendingTimeRanges()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query ;
    query << "select * from " << CDPV1PENDINGTIMERANGETBLNAME << " order by c_entryno ASC";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchAppliedTableEntry(const CDPV1AppliedDiffInfo & entry)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    stringstream query;
    query << "select * from " << CDPV1APPLIEDTBLNAME
        << " where c_difffile='" << entry.c_difffile << "'"
        << " AND c_startoffset=" << entry.c_startoffset
        << " AND c_endoffset=" << entry.c_endoffset
        << " ;" ;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query.str());
}

CDPV1Database::Reader CDPV1Database::FetchQueryResult(const string & query)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Reader(*this,query);
}

bool CDPV1Database::Insert(const CDPV1DataDir & dir)
{	
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        // PR#10815: Long Path support
        char cdirname[SV_MAX_PATH];

        inm_strcpy_s(cdirname, ARRAYSIZE(cdirname), dir.c_dir.c_str());
        PathRemoveAllBackslashes(cdirname);

        stringstream  action;
        action << "insert into " << CDPV1DATADIRTBLNAME
            << " ( c_dir,c_freespace,c_usedspace,c_allocated, c_tsfc, c_tslc )"
            << " values("
            << "'" << cdirname  << "'"  << ","
            << dir.c_freespace           << ","
            << dir.c_usedspace           << ","
            << dir.c_allocated           << ","
            << dir.c_tsfc                << ","
            << dir.c_tslc
            << " );" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during insert\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1DATADIRTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Insert(const CDPV1Inode & inode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream tmpstream, action;

        CDPV1DiffInfosConstIterator iter = inode.c_diffinfos.begin();
        CDPV1DiffInfosConstIterator end = inode.c_diffinfos.end();

        for ( ; iter != end; ++iter )
        {
            CDPV1DiffInfoPtr diffInfoPtr = *iter;

            tmpstream << diffInfoPtr ->start 
                << "," << diffInfoPtr -> end
                << "," << diffInfoPtr -> tsfcseq
                << "," << diffInfoPtr -> tsfc
                << "," << diffInfoPtr -> tslcseq
                << "," << diffInfoPtr -> tslc
                << "," << diffInfoPtr -> ctid
                << "," << diffInfoPtr -> numchanges;

            cdp_drtdv2s_t changes = diffInfoPtr -> changes;
            cdp_drtdv2s_iter_t change_iter = changes.begin();
            cdp_drtdv2s_iter_t change_end = changes.end();
            for ( ; change_iter != change_end; ++change_iter)
            {
                cdp_drtdv2_t change = *change_iter;
                tmpstream << "-" 
                    << change.get_volume_offset()
                    << "," << change.get_length()
                    << "," << change.get_file_offset();

                // we will store per i/o information 
                // only if the inode supports it
                if(inode.c_version >= CDPV1_INODE_PERIOVERSION)
                {
                    tmpstream << "," 
                        << change.get_seqdelta()
                        << "," << change.get_timedelta();
                }
            }

            tmpstream << ";";
        }


        action << "insert into "  << CDPV1INODETBLNAME
            << " ( c_type,c_dirnum, c_datafile, c_startseq, c_starttime, c_endseq, c_endtime,"
            << " c_logicalsize,c_physicalsize, c_numdiffs, c_diffinfos, c_numevents, c_version )"
            << " values(" 
            << "'" << inode.c_type  << "'"  << "," 
            << "'" << inode.c_dirnum  << "'"  << "," 
            << "'" << inode.c_datafile << "'"  << "," 
            << inode.c_startseq                << "," 
            << inode.c_starttime               << ","
            << inode.c_endseq                  << "," 
            << inode.c_endtime                 << "," 
            << inode.c_logicalsize             << "," 
            << inode.c_physicalsize            << "," 
            << inode.c_numdiffs                << ","
            << "'" << tmpstream.str()   << "'"  << ","
            << inode.c_numevents << ","
            << inode.c_version
            << ")";

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during insert\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1INODETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Insert(const CDPV1Event & event)
{	
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream  action;
        action << "insert into " << CDPV1EVENTTBLNAME
            << " ( c_inodeno,c_eventtype,c_eventtime,c_eventvalue,c_eventmode,c_identifier,c_verified,c_comment )"
            << " values("
            << event.c_inodeno     << ","
            << event.c_eventtype   << ","
            << event.c_eventtime   << ","
            << '?' << ","
            << event.c_eventmode << ","
            << '?' << ","
            << event.c_verified << ","
            << '?' << " );" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.bind( 1, event.c_eventvalue );
        cmd.bind(2, event.c_identifier);
        cmd.bind(3, event.c_comment);
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during insert\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1EVENTTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

        rv = false;
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Insert(const CDPV1PendingEvent & event)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
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
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during insert\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1PENDINGEVENTTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Insert(const CDPV1AppliedDiffInfo & entry)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "insert into "  << CDPV1APPLIEDTBLNAME 
            << " ( c_difffile, c_startoffset, c_endoffset )"
            << " values(" 
            << "'" << entry.c_difffile << "'" 
            << "," << entry.c_startoffset 
            << "," << entry.c_endoffset
            << " );" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during insert\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1APPLIEDTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



bool CDPV1Database::Insert(const CDPV1TimeRange & timerange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream  action;
        action << "insert into " << CDPV1TIMERANGETBLNAME
            << " ( c_starttime,c_endtime,c_mode,c_startseq,c_endseq )"
            << " values("
            << timerange.c_starttime   << ","
            << timerange.c_endtime     << ","
            << timerange.c_mode		   << ","
            << timerange.c_startseq    << ","
            << timerange.c_endseq
            << " );" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during insert\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1TIMERANGETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV1Database::Insert(const CDPV1PendingTimeRange & pendingtimerange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream  action;
        action << "insert into " << CDPV1PENDINGTIMERANGETBLNAME
            << " ( c_starttime,c_endtime,c_mode,c_startseq,c_endseq )"
            << " values("
            << pendingtimerange.c_starttime   << ","
            << pendingtimerange.c_endtime     << ","
            << pendingtimerange.c_mode	      << ","
            << pendingtimerange.c_startseq    << ","
            << pendingtimerange.c_endseq
            << " );" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during insert\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1PENDINGTIMERANGETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



bool CDPV1Database::Add(CDPV1TimeRange &timerange)
{
    bool rv = true;

    //__asm int 3
    do
    {    //get last row      
        stringstream query;
        query << "select * from " << CDPV1TIMERANGETBLNAME 
            << " order by c_entryno DESC limit 1";

        CDPV1TimeRange lasttimerange ;
        CDPV1Database::Reader reader = FetchQueryResult(query.str());

        SVERROR hr_read = reader.read(lasttimerange);
        reader.close();

        // on failure break
        if (hr_read.failed())
        {
            rv = false;
            break;
        }

        // on successfully reading the last entry
        if( hr_read== SVS_OK)
        {               
            // update the last entry if the modes are same
            if(lasttimerange.c_mode == timerange.c_mode)  
            {
                timerange.c_starttime=lasttimerange.c_starttime;
                timerange.c_startseq =lasttimerange.c_startseq;
                timerange.c_entryno=lasttimerange.c_entryno;             
                if(!Update(timerange))   
                {
                    rv = false;
                    break;
                }                
            }
            else // modes are different, insert new entry
            {
                if(!Insert(timerange))
                {
                    rv = false;
                    break;
                } 
            }

        }
        else // no entries in the timerange table
        { 
            if(!Insert(timerange))
            {
                rv = false;
                break;
            }                    
        }

    }while(0);

    return rv;
}


bool CDPV1Database::Add(CDPV1PendingTimeRange &pendingtimerange)
{
    bool rv=true;   
    do
    {
        //get last row
        stringstream query;
        query << "select * from " 
            << CDPV1PENDINGTIMERANGETBLNAME
            << " order by c_entryno DESC limit 1";

        CDPV1PendingTimeRange lasttimerange ;
        CDPV1Database::Reader reader=FetchQueryResult(query.str());

        SVERROR hr_read = reader.read(lasttimerange);
        reader.close();

        // on failure break
        if (hr_read.failed())
        {
            rv = false;
            break;
        }

        // on successfully reading the last entry
        if(hr_read == SVS_OK)
        {
            // update the last entry if the modes are same
            if(lasttimerange.c_mode == pendingtimerange.c_mode)   
            {
                pendingtimerange.c_starttime=lasttimerange.c_starttime;
                pendingtimerange.c_startseq=lasttimerange.c_startseq;
                pendingtimerange.c_entryno=lasttimerange.c_entryno;              
                if(!Update(pendingtimerange))   
                {
                    rv = false;
                    break;
                }                
            }
            else
            {    // modes are different, insert new entry
                if(!Insert(pendingtimerange))
                {
                    rv = false;
                    break;
                } 
            }
        }
        else
        {
            // no entries in the timerange table          
            if(!Insert(pendingtimerange))
            {
                rv = false;
                break;
            }                    
        }
    }while(0);
    return rv;
}


bool CDPV1Database::Update(const CDPV1SuperBlock & superblock)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
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
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during update\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1SUPERBLOCKTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV1Database::Update(const CDPV1TimeRange &timerange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream tmpstream, action;

        action << "update " << CDPV1TIMERANGETBLNAME
            << " set "
            << " c_starttime=" << timerange.c_starttime << ","			
            << " c_endtime=" << timerange.c_endtime << ","
            << " c_mode=" << timerange.c_mode 	<< ","
            << " c_startseq=" << timerange.c_startseq << "," 
            << " c_endseq=" << timerange.c_endseq
            << " where c_entryno=" << timerange.c_entryno << ";" ;          

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during update\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1TIMERANGETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Update(const CDPV1PendingTimeRange &pendingtimerange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream tmpstream, action;

        action << "update " << CDPV1PENDINGTIMERANGETBLNAME
            << " set "			   		
            << " c_starttime=" << pendingtimerange.c_starttime << ","			
            << " c_endtime=" << pendingtimerange.c_endtime   << ","
            << " c_mode=" << pendingtimerange.c_mode         << ","
            << " c_startseq=" << pendingtimerange.c_startseq << "," 
            << " c_endseq=" << pendingtimerange.c_endseq
            << " where c_entryno=" << pendingtimerange.c_entryno << ";" ;          

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during update\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1PENDINGTIMERANGETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Update(const CDPV1Inode &inode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream tmpstream, action;

        CDPV1DiffInfosConstIterator iter = inode.c_diffinfos.begin();
        CDPV1DiffInfosConstIterator end = inode.c_diffinfos.end();

        for ( ; iter != end; ++iter )
        {
            CDPV1DiffInfoPtr diffInfoPtr = *iter;

            tmpstream << diffInfoPtr ->start 
                << "," << diffInfoPtr -> end
                << "," << diffInfoPtr -> tsfcseq
                << "," << diffInfoPtr -> tsfc
                << "," << diffInfoPtr -> tslcseq
                << "," << diffInfoPtr -> tslc
                << "," << diffInfoPtr -> ctid
                << "," << diffInfoPtr -> numchanges;

            cdp_drtdv2s_t changes = diffInfoPtr -> changes;
            cdp_drtdv2s_iter_t change_iter = changes.begin();
            cdp_drtdv2s_iter_t change_end = changes.end();
            for ( ; change_iter != change_end; ++change_iter)
            {
                cdp_drtdv2_t change = *change_iter;
                tmpstream << "-" 
                    << change.get_volume_offset()
                    << "," << change.get_length()
                    << "," << change.get_file_offset();

                // we will store per i/o information 
                // only if the inode supports it
                if(inode.c_version >= CDPV1_INODE_PERIOVERSION)
                {
                    tmpstream << "," 
                        << change.get_seqdelta()
                        << "," << change.get_timedelta();
                }
            }

            tmpstream << ";";
        }

        action << "update " << CDPV1INODETBLNAME
            << " set "
            << " c_type=" << inode.c_type << ","
            << " c_dirnum=" << inode.c_dirnum << ","
            << " c_datafile='" << inode.c_datafile << "'" << ","
            << " c_startseq=" << inode.c_startseq << ","
            << " c_starttime=" << inode.c_starttime << ","
            << " c_endseq=" << inode.c_endseq << ","
            << " c_endtime=" << inode.c_endtime << ","
            << " c_physicalsize=" << inode.c_physicalsize << ","
            << " c_logicalsize="  << inode.c_logicalsize << ","
            << " c_numdiffs="     << inode.c_numdiffs << ","
            << " c_diffinfos='" << tmpstream.str() << "'" << ","
            << " c_numevents="  << inode.c_numevents << ","
            << " c_version=" << inode.c_version 
            << " where c_inodeno=" << inode.c_inodeno << ";" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during update\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1INODETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::RevokeTags(const SV_ULONGLONG & inodeNo)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream inode_update, event_update, pendingevent_update;

        inode_update << "update " << CDPV1INODETBLNAME
            << " set "
            << " c_numevents=0"   
            << " where c_inodeno=" << inodeNo << ";" ;

        sqlite3_command cmd1(*m_con, inode_update.str() );
        cmd1.executenonquery();

        event_update << "delete from  " << CDPV1EVENTTBLNAME
            << " where c_inodeno=" << inodeNo << ";" ;

        sqlite3_command cmd2(*m_con, event_update.str() );
        cmd2.executenonquery();

        pendingevent_update << "update " << CDPV1PENDINGEVENTTBLNAME
            << " set c_valid=0" 
            << " where c_inodeno=" << inodeNo << ";" ;

        sqlite3_command cmd3(*m_con, pendingevent_update.str() );
        cmd3.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during update\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1INODETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV1Database::Delete(const CDPV1Inode & inode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "delete from "  << CDPV1INODETBLNAME 
            << " where c_inodeno="
            << inode.c_inodeno
            << ";" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during delete\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1INODETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Delete(const CDPV1Event & event)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "delete from "  << CDPV1EVENTTBLNAME 
            << " where c_eventno="
            << event.c_eventno
            << ";" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during delete\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1EVENTTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Delete(const CDPV1PendingEvent & pendingevent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "delete from "  << CDPV1PENDINGEVENTTBLNAME 
            << " where c_eventno="
            << pendingevent.c_eventno
            << ";" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during delete\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1PENDINGEVENTTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV1Database::Delete(const CDPV1AppliedDiffInfo & entry)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "delete from "  << CDPV1APPLIEDTBLNAME 
            << " where c_difffile="
            << "'" << entry.c_difffile << "'"
            << " AND c_startoffset=" << entry.c_startoffset
            << " AND c_endoffset=" << entry.c_endoffset
            << " ;" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during delete\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1APPLIEDTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Delete(const CDPV1TimeRange &timerange )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "delete from "  << CDPV1TIMERANGETBLNAME 
            << " where c_entryno = "
            << timerange.c_entryno << ";";

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during delete\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1TIMERANGETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;

}

bool CDPV1Database::Delete(const CDPV1PendingTimeRange &pendingtimerange )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "delete from "  << CDPV1PENDINGTIMERANGETBLNAME 
            << " where c_entryno = "
            << pendingtimerange.c_entryno << ";";

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during delete\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1PENDINGTIMERANGETBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;

}
bool CDPV1Database::DeleteAppliedEntries(const string & difffile)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    try
    {
        stringstream action;
        action << "delete from "  << CDPV1APPLIEDTBLNAME 
            << " where c_difffile="
            << "'" << difffile << "'"
            << " ;" ;

        sqlite3_command cmd(*m_con, action.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
            << "Exception Occured during delete\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << CDPV1APPLIEDTBLNAME    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

CDPV1Database::Reader::Reader(CDPV1Database & db, const string & query)
:m_db(db),m_query(query)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        try
        {
            sqlite3_command * cmd = new sqlite3_command(*(m_db.m_con),query);
            if(!cmd)
            {
                // you will never reach here due to exception
                m_rv = false;
                break;
            }

            m_cmd.reset(cmd);
            m_reader= m_cmd -> executereader();
            m_rv = true;
        }
        catch(exception &ex)
        {
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured during read operation\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Query: "     << m_query << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            m_rv = false;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

SVERROR CDPV1Database::Reader::read(CDPV1SuperBlock & superblock)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                break;
            }

            superblock.c_type = static_cast<CDP_LOGTYPE> (m_reader.getint(SUPERBLOCK_C_TYPE_COL));
            superblock.c_lookuppagesize = m_reader.getint64(SUPERBLOCK_C_LOOKUPPAGESIZE_COL);
            superblock.c_physicalspace = m_reader.getint64(SUPERBLOCK_C_PHYSICALSPACE_COL);
            superblock.c_tsfc = m_reader.getint64(SUPERBLOCK_C_TSFC_COL);
            superblock.c_tslc = m_reader.getint64(SUPERBLOCK_C_TSLC_COL);
            superblock.c_numinodes = m_reader.getint64(SUPERBLOCK_C_NUMINODES_COL);

            SV_EVENT_TYPE EventType;
            SV_ULONGLONG NumEvents;

            superblock.c_eventsummary.clear();
            string eventsummarystring = m_reader.getblob(SUPERBLOCK_C_EVENTSUMMARY_COL);
            svector_t eventinfos = CDPUtil::split(eventsummarystring, ";" );
            for ( unsigned int eventnum =0; eventnum < eventinfos.size() ; ++eventnum)
            {
                string tmp = eventinfos[eventnum];
                sscanf(tmp.c_str(), "%hu," LLSPEC, &EventType, &NumEvents);
                superblock.c_eventsummary[EventType] = NumEvents;
            }

            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "Exception Occured while reading superblock information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1SUPERBLOCKTBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    close();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

bool CDPV1Database::Reader::read(vector<CDPV1Event> & events)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        if(!m_rv)
        {
            rv = false;
            break;
        }

        events.clear();
        CDPV1Event event;
        SVERROR hr_event;
        while ( (hr_event = read(event)) == SVS_OK )
        {
            events.push_back(event);
        }

        if(hr_event.failed())
        {
            rv = false;
            break;
        }

        if(hr_event == SVS_FALSE)
        {
            rv = true;
            break;
        }

    } while (FALSE);

    close();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Reader::read(vector<CDPV1PendingEvent> & PendingEvents)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        if(!m_rv)
        {
            rv = false;
            break;
        }

        PendingEvents.clear();
        CDPV1PendingEvent pendingEvent;

        SVERROR hr_pendingEvent;
        while ( (hr_pendingEvent = read(pendingEvent)) == SVS_OK )
        {
            PendingEvents.push_back(pendingEvent);
        }

        if(hr_pendingEvent.failed())
        {
            rv = false;
            break;
        }

        if(hr_pendingEvent == SVS_FALSE)
        {
            rv = true;
            break;
        }

    } while (FALSE);

    close();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Reader::read(vector<CDPV1TimeRange> & TimeRanges )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        if(!m_rv)
        {
            rv = false;
            break;
        }

        TimeRanges.clear();
        CDPV1TimeRange timerange;

        SVERROR hr_timerange;
        while ( (hr_timerange = read(timerange)) == SVS_OK )
        {
            TimeRanges.push_back(timerange);
        }

        if(hr_timerange.failed())
        {
            rv = false;
            break;
        }

        if(hr_timerange == SVS_FALSE)
        {
            rv = true;
            break;
        }

    } while (FALSE);

    close();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1Database::Reader::read(vector<CDPV1PendingTimeRange> & PendingTimeRanges )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        if(!m_rv)
        {
            rv = false;
            break;
        }

        PendingTimeRanges.clear();
        CDPV1PendingTimeRange pendingtimerange;

        SVERROR hr_pendingtimerange;
        while ( (hr_pendingtimerange = read(pendingtimerange)) == SVS_OK )
        {
            PendingTimeRanges.push_back(pendingtimerange);
        }

        if(hr_pendingtimerange.failed())
        {
            rv = false;
            break;
        }

        if(hr_pendingtimerange == SVS_FALSE)
        {
            rv = true;
            break;
        }

    } while (FALSE);

    close();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}
bool CDPV1Database::Reader::read(vector<CDPV1DataDir> & datadirs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        if(!m_rv)
        {
            rv = false;
            break;
        }

        datadirs.clear();
        CDPV1DataDir datadir;
        SVERROR hr_datadir;
        while ( (hr_datadir = read(datadir)) == SVS_OK)
        {
            datadirs.push_back(datadir);
        }

        if(hr_datadir.failed())
        {
            rv = false;
            break;
        }

        if(hr_datadir == SVS_FALSE)
        {
            rv = true;
            break;
        }

    } while (FALSE);

    close();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SVERROR CDPV1Database::Reader::read(CDPV1DataDir & datadir)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                close();
                break;
            }

            datadir.c_dirnum = m_reader.getint(DATADIR_C_DIRNUM_COL);
            datadir.c_dir    = m_reader.getblob(DATADIR_C_DIR_COL);
            datadir.c_freespace    = m_reader.getint64(DATADIR_C_FREESPACE_COL);
            datadir.c_usedspace    = m_reader.getint64(DATADIR_C_USEDSPACE_COL);
            datadir.c_allocated    = m_reader.getint64(DATADIR_C_ALLOCATED_COL);
            datadir.c_tsfc         = m_reader.getint64(DATADIR_C_TSFC_COL);
            datadir.c_tslc         = m_reader.getint64(DATADIR_C_TSLC_COL);
            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured while reading datadir information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1DATADIRTBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

SVERROR CDPV1Database::Reader::read(CDPV1Inode & inode, bool ReadDiff)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        //__asm int 3;
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                close();
                break;
            }

            inode.c_diffinfos.clear();

            inode.c_inodeno = m_reader.getint64(INODE_C_INODENO_COL);
            inode.c_type  = m_reader.getint(INODE_C_TYPE_COL);
            inode.c_dirnum  = m_reader.getint(INODE_C_DIRNUM_COL);

            if( inode.c_dirnum)
            {

                CDPV1DataDir datadir;
                CDPV1Database::Reader readdatadir = m_db.FetchDataDirForDirNum(inode.c_dirnum);
                if(readdatadir.read(datadir) != SVS_OK)
                {
                    hr = SVE_FAIL;
                    break;
                }
                inode.c_datadir = datadir.c_dir;
            }
            else
            {
                inode.c_datadir = m_db.m_dbdir;
            }

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
            double dbrev;           
            m_db.GetRevision(dbrev);
            if(dbrev >= CDPVER1REV2)
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
                    << "Database: " << m_db.m_dbname 
                    << " is corrupted. Differentials will not be applied.\n"
                    << "Error Occured while reading inode information "
                    << "invalid inode version\n"
                    << "inodeno: " << inode.c_inodeno << "\n"
                    << "version" << inode.c_version << "\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                return  SVE_FAIL;
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
                        << "Database: "<< m_db.m_dbname 
                        << " is corrupted. Differentials will not be applied.\n"
                        << "Error Occured while reading inode information "
                        << "(diffinfos.size != inode.c_numdiffs )\n"
                        << "diffinfos.size is " << diffinfos.size() << "\n"
                        << "inode.c_numdiffs is " << inode.c_numdiffs << "\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    return  SVE_FAIL;
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
                            << "Database: "<< m_db.m_dbname 
                            << " is corrupted. Differentials will not be applied.\n"
                            << "Improper String format. Cannot read from string:"
                            << tmp <<"\n";
                        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                        return  SVE_FAIL;       
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
                            << "Database: " << m_db.m_dbname 
                            << " is corrupted. Differentials will not be applied.\n"
                            << "Error Occured while reading inode information" 
                            << "(changeinfos.size() != info -> numchanges + 1 )\n"
                            << "changeinfos.size is "<< changeinfos.size()
                            << " info -> numchanges is "<< info -> numchanges  << "\n";

                        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                        return  SVE_FAIL;
                    }

                    for (unsigned int j = 0; j < info -> numchanges ; ++j)
                    {
                        SV_UINT         length = 0;
                        SV_OFFSET_TYPE  voloffset = 0;
                        SV_OFFSET_TYPE  fileoffset = 0;
                        SV_UINT      	seqdelta = 0;
                        SV_UINT      	timedelta = 0;


                        string change_tmp = changeinfos[j+1];
                        commas = count(change_tmp.begin(), change_tmp.end(), ',');


                        if (inode.c_version == CDPV1_INODE_VERSION0)
                        {

                            if ( commas != 2 )
                            {
                                stringstream l_stderr;
                                l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
                                    << "Database: "<< m_db.m_dbname 
                                    << " is corrupted. Differentials will not be applied.\n"
                                    << "Improper String format. Cannot read from string:"
                                    << tmp<< "\n";

                                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                                return  SVE_FAIL;       
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
                                    << "Database: "<< m_db.m_dbname 
                                    << " is corrupted. Differentials will not be applied.\n"
                                    << "Improper String format. Cannot read from string:"
                                    << tmp<< "\n";

                                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                                return  SVE_FAIL;       
                            }

                            sscanf(change_tmp.c_str(), LLSPEC ",%d," LLSPEC ",%d,%d", 
                                &(voloffset), 
                                &(length), 
                                &(fileoffset),
                                &(seqdelta),
                                &(timedelta));

                        }

                        cdp_drtdv2_t change(length, voloffset,fileoffset,seqdelta,timedelta,0);
                        info -> changes.push_back(change);		
                    }
                    inode.c_diffinfos.push_back(info);
                }
            }

            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "Exception Occured while reading inode information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1INODETBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}


SVERROR CDPV1Database::Reader::read(CDPV1Event & event)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                close();
                break;
            }

            double rev;           
            m_db.GetRevision(rev);
            if(rev >= CDPVER1REV1)
            {
                event.c_eventmode = 
                    static_cast<SV_USHORT> (m_reader.getint(EVENT_C_EVENTMODE_COL));
            }
            else
            {
                event.c_eventmode = 1; 
            }
            if(rev >= CDPVER1REV3)
            {
                event.c_identifier = m_reader.getblob(EVENT_C_IDENTIFIER_COL);
                event.c_verified = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_VERIFIED_COL));
                event.c_comment = m_reader.getblob(EVENT_C_COMMENT_COL);
            }
            event.c_eventno = m_reader.getint64(EVENT_C_EVENTNO_COL);
            event.c_inodeno = m_reader.getint64(EVENT_C_INODENO_COL);
            event.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(EVENT_C_EVENTTYPE_COL));
            event.c_eventtime = m_reader.getint64(EVENT_C_EVENTTIME_COL);
            event.c_eventvalue = m_reader.getblob(EVENT_C_EVENTVALUE_COL);         
            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured while reading event information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1EVENTTBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}


SVERROR CDPV1Database::Reader::read(CDPV1TimeRange & TimeRange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                close();
                break;
            }

            TimeRange.c_entryno = m_reader.getint64(TIMERANGE_C_ENTRYNO_COL);
            TimeRange.c_starttime = m_reader.getint64(TIMERANGE_C_STARTTIME_COL);
            TimeRange.c_mode = static_cast<SV_USHORT> (m_reader.getint(TIMERANGE_C_MODE_COL));
            TimeRange.c_endtime = m_reader.getint64(TIMERANGE_C_ENDTIME_COL);

            double rev;           
            m_db.GetRevision(rev);
            if(rev >= CDPVER1REV2)
            {
                // c_startseq and c_endseq were added only in revision 2.0
                // these columns were not available in earlier database revision
                TimeRange.c_startseq =  m_reader.getint64(TIMERANGE_C_STARTSEQ_COL);
                TimeRange.c_endseq =  m_reader.getint64(TIMERANGE_C_ENDSEQ_COL);
            }
            else
            {
                TimeRange.c_startseq = 0;
                TimeRange.c_endseq = 0;
            }
            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "Exception Occured while reading time classification information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1TIMERANGETBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

SVERROR CDPV1Database::Reader::read(CDPV1PendingTimeRange & PendingTimeRange)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                close();
                break;
            }

            PendingTimeRange.c_entryno = m_reader.getint64(PENDINGTIMERANGE_C_ENTRYNO_COL);
            PendingTimeRange.c_starttime = m_reader.getint64(PENDINGTIMERANGE_C_STARTTIME_COL);
            PendingTimeRange.c_endtime = m_reader.getint64(PENDINGTIMERANGE_C_ENDTIME_COL);
            PendingTimeRange.c_mode = static_cast<SV_USHORT> (m_reader.getint(PENDINGTIMERANGE_C_MODE_COL));

            double rev;           
            m_db.GetRevision(rev);
            if(rev >= CDPVER1REV2)
            {
                // c_startseq and c_endseq were added only in revision 2.0
                // these columns were not available in earlier database revision
                PendingTimeRange.c_startseq =  m_reader.getint64(TIMERANGE_C_STARTSEQ_COL);
                PendingTimeRange.c_endseq =  m_reader.getint64(TIMERANGE_C_ENDSEQ_COL);
            }
            else
            {
                PendingTimeRange.c_startseq = 0;
                PendingTimeRange.c_endseq = 0;
            }
            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "Exception Occured while reading time classification information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1PENDINGTIMERANGETBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

SVERROR CDPV1Database::Reader::read(CDPV1PendingEvent & pendingevent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                close();
                break;
            }

            double rev;           
            m_db.GetRevision(rev);
            if(rev >= CDPVER1REV1)
            {
                pendingevent.c_eventmode = 
                    static_cast<SV_USHORT> (m_reader.getint(PENDINGEVENT_C_EVENTMODE_COL));
            }
            else
            {
                pendingevent.c_eventmode = 1; 
            }
            if(rev >= CDPVER1REV3)
            {
                pendingevent.c_identifier = m_reader.getblob(PENDINGEVENT_C_IDENTIFIER_COL);
                pendingevent.c_verified = static_cast<SV_USHORT> (m_reader.getint(PENDINGEVENT_C_VERIFIED_COL));
                pendingevent.c_comment = m_reader.getblob(PENDINGEVENT_C_COMMENT_COL);
            }

            pendingevent.c_eventno = m_reader.getint64(PENDINGEVENT_C_EVENTNO_COL);
            pendingevent.c_inodeno = m_reader.getint64(PENDINGEVENT_C_INODENO_COL);
            pendingevent.c_eventtype = static_cast<SV_USHORT> (m_reader.getint(PENDINGEVENT_C_EVENTTYPE_COL));
            pendingevent.c_eventtime = m_reader.getint64(PENDINGEVENT_C_EVENTTIME_COL);
            pendingevent.c_eventvalue = m_reader.getblob(PENDINGEVENT_C_EVENTVALUE_COL);
            pendingevent.c_valid      = (m_reader.getint(PENDINGEVENT_C_VALID_COL) == 1);               
            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "Exception Occured while reading pending event information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1PENDINGEVENTTBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}

SVERROR CDPV1Database::Reader::read(CDPV1AppliedDiffInfo & entry)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR hr = SVE_FAIL;

    do
    {
        if(!m_rv)
        {
            hr = SVE_FAIL;
            break;
        }

        try
        {
            if(!m_reader.read())
            {
                hr = SVS_FALSE;
                close();
                break;
            }

            entry.c_difffile = m_reader.getstring(APPLIED_C_DIFFFILE_COL);
            entry.c_startoffset = m_reader.getint64(APPLIED_C_STARTOFFSET_COL);
            entry.c_endoffset = m_reader.getint64(APPLIED_C_ENDOFFSET_COL);

            hr = SVS_OK;
        }
        catch(exception &ex){
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured while reading Applied Table information\n"
                << " Database : " << m_db.m_dbname << "\n"
                << " Table : " << CDPV1APPLIEDTBLNAME << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*(m_db.m_con)).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            hr = SVE_FAIL;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return hr;
}


bool CDPV1Database:: AdjustTimerangeTable(CDPV1SuperBlock &superblock )
{
    bool rv;
    do{
        // 1 Delete if end time < first time
        // 2 Update start time = first time
        //          where  start time < first time and  end time > first time
        try
        {
            stringstream delquery;
            delquery<< " delete from "<< CDPV1TIMERANGETBLNAME << " where c_endtime <" 
                << superblock.c_tsfc;

            sqlite3_command cmd(*m_con, delquery.str() );
            cmd.executenonquery();
            rv = true;
        }
        catch(exception &ex) {
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured during delete\n"
                << " Database : " << m_dbname << "\n"
                << " Table :"     << CDPV1TIMERANGETBLNAME    << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*m_con).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        try
        {
            stringstream updquery;
            updquery<< "update " << CDPV1TIMERANGETBLNAME
                << " set "
                << " c_starttime = " << superblock.c_tsfc 					                  
                << " where c_starttime < " << superblock.c_tsfc << " and "
                << " c_endtime > " << superblock.c_tsfc << ";" ;

            sqlite3_command cmd(*m_con, updquery.str() );
            cmd.executenonquery();
            rv = true;
        }
        catch(exception &ex) {
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured during delete\n"
                << " Database : " << m_dbname << "\n"
                << " Table :"     << CDPV1TIMERANGETBLNAME    << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*m_con).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }           
    }while(0);
    return rv;
}


bool CDPV1Database:: AdjustPendingTimerangeTable(CDPV1SuperBlock &superblock )
{
    bool rv;
    do{
        try
        {
            stringstream delquery;
            delquery<< " delete from "
                << CDPV1PENDINGTIMERANGETBLNAME 
                << " where c_endtime < "  << superblock.c_tsfc;

            sqlite3_command cmd(*m_con, delquery.str() );
            cmd.executenonquery();
            rv = true;
        }
        catch(exception &ex) {
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured during delete\n"
                << " Database : " << m_dbname << "\n"
                << " Table :"     << CDPV1PENDINGTIMERANGETBLNAME    << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*m_con).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        try
        {
            stringstream updquery;
            updquery<< "update " << CDPV1PENDINGTIMERANGETBLNAME
                << " set "
                << " c_starttime = " << superblock.c_tsfc 				                  
                << " where c_starttime < " << superblock.c_tsfc << " and "
                << " c_endtime > " << superblock.c_tsfc << ";" ;

            sqlite3_command cmd(*m_con, updquery.str() );
            cmd.executenonquery();
            rv = true;
        }
        catch(exception &ex) {
            stringstream l_stderr;
            l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Exception Occured during delete\n"
                << " Database : " << m_dbname << "\n"
                << " Table :"     << CDPV1PENDINGTIMERANGETBLNAME    << "\n"
                << " Exception :"  << ex.what() << "\n" 
                << " Exception :" << (*m_con).errmsg() << "\n"
                << USER_DEFAULT_ACTION_MSG << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }           
    }while(0);
    return rv;
}

bool CDPV1Database:: DeleteAllRows(const string &tblname)
{
    bool rv;
    try
    {

        stringstream deleteall; 
        deleteall << "delete from " << tblname ;

        sqlite3_command cmd(*m_con, deleteall.str() );
        cmd.executenonquery();
        rv = true;
    }
    catch(exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Exception Occured during deleteing all rows\n"
            << " Database : " << m_dbname << "\n"
            << " Table :"     << tblname    << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n" ;

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;

    }
    return rv;
}

void CDPV1Database::Reader::close()
{
    m_reader.close();
}

bool CDPV1Database::redate(const std::string & tblname, std::vector<std::string> & columns)
{
    bool rv = false;

    try {

        string input = "2100/12/31";
        SV_ULONGLONG futureTime = 0;
        CDPUtil::InputTimeToFileTime(input,futureTime);

        input = "2000/1/1";
        SV_ULONGLONG pastTime = 0;
        CDPUtil::InputTimeToFileTime(input,pastTime);

        SV_ULONGLONG epochdiff = (static_cast<SV_ULONGLONG>(116444736) * static_cast<SV_ULONGLONG>(1000000000));

        std::vector<std::string>::iterator vIter = columns.begin();
        std::vector<std::string>::iterator vEnd = columns.end();

        while (vIter != vEnd)
        {
            stringstream query;
            std::string column =*vIter;

            query << " update " << tblname
                << " set "
                << column << "=" << column << "-" << epochdiff
                << " where " << column << ">" << futureTime ;
            (*m_con).executenonquery(query.str());

            query.str("");
            query << " update " << tblname
                << " set "
                << column << "=" << column << "+" << epochdiff
                << " where " << column << "<" << pastTime ;
            (*m_con).executenonquery(query.str());

            ++vIter;
        }

        rv=true;
    }catch (exception &ex) {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME <<"\n"
            << "Exception Occured while redating :" 
            << " Database : " << m_dbname << "\n"
            << " Table : " << tblname << "\n"
            << " Exception :"  << ex.what() << "\n" 
            << " Exception :" << (*m_con).errmsg() << "\n";


        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;              
    }
    return rv;
}
