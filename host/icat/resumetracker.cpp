#include "resumetracker.h"
#include "logger.h"
#include <sstream>
#include <time.h>
#include "common.h"
#include <algorithm>
#include "archivecontroller.h"
#define MASTERTABLE			"t_master"
#define JOBSTATUSTABLE		"t_jobstatus"		
#define DIRECTORYSTATUS    "t_directorystatus"
#define FILESTATUS				"t_filestatus"




ResumeTracker::ResumeTracker(const std::string & branchName, 
							const std::string& serverName, 
							const std::string& volName,
							const std::string& targetPath, 
							unsigned int maxCopies,
							unsigned int maxDays,
							ICAT_RESUME_TRACKER_MODE trackMode)

{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_branchName = branchName ;
	m_serverName = serverName ;
	m_volName = volName ;
	m_tgtPath = targetPath ;
	m_mode = trackMode ;
	m_maxCopies = maxCopies ;
	m_maxDays =  maxDays ;
	m_tgtPath.erase( m_tgtPath .find_last_not_of("/") + 1) ; 
	m_tgtPath.erase(0 , m_tgtPath .find_first_not_of("/") ) ;
	m_tgtPath.erase( m_tgtPath .find_last_not_of("\\") + 1) ; 
	m_tgtPath.erase(0 , m_tgtPath .find_first_not_of("\\") ) ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;

}


ICAT_RESUME_TRACKER_MODE  ResumeTracker::mode()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return m_mode ;
}


void SqliteResumeTracker::initMasterInfo()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream select_query, create_query ;

	select_query << " SELECT name FROM sqlite_master WHERE type=\'table\' AND name=\'t_master\' ;" ;
	
	create_query << " CREATE TABLE t_master  " ;
	create_query << "(\"c_configid\" INTEGER PRIMARY KEY AUTOINCREMENT , " ;
	create_query << "\"c_remoteoffice\" TEXT NOT NULL ,   " ;
	create_query << "\"c_servername\" TEXT NOT NULL ,  " ;
	create_query << "\"c_sourcevolume\" TEXT NOT NULL ) ; " ;
	
	m_con.execQuery(select_query.str(), set) ;
	if( set.getSize() == 0 )
	{
		m_con.execQuery(create_query.str(), set) ;
		std::stringstream indexing_query ;
		indexing_query << "CREATE INDEX masterIdx1 ON t_master ( c_configid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;

	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::initJobStatusInfo()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream select_query, create_query ;
	select_query << "SELECT  name FROM sqlite_master WHERE type=\'table\' AND name=\'t_jobstatus\' " ;
	
	create_query << "CREATE TABLE t_jobstatus " ;
	create_query << "(\"c_configid\" INTEGER NOT NULL , " ;
	create_query << "\"c_instanceid\" INTEGER PRIMARY KEY AUTOINCREMENT , " ;
	create_query << "\"c_destinationdirectory\" TEXT NOT NULL , " ;
	create_query << "\"c_starttime\" DATETIME NOT NULL , " ;
	create_query << "\"c_endtime\" DATETIME NOT NULL , " ;
	create_query << "\"c_status\"	INTEGER NOT NULL ); " ;

	m_con.execQuery(select_query.str(), set) ;
	if( set.getSize() == 0 )
	{
		m_con.execQuery(create_query.str() , set) ;
		DebugPrintf(SV_LOG_DEBUG, "Created the t_jobstatus in icat.db\n") ;
		std::stringstream indexing_query ;
		indexing_query << "CREATE INDEX jobStatusIdx1 ON t_jobstatus ( c_instanceid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;
		indexing_query.str("") ;
		indexing_query << "CREATE INDEX jobStatusIdx2 ON t_jobstatus ( c_configid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::initDirStatusInfo()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream select_query, create_query ;
	select_query << "SELECT name FROM sqlite_master WHERE type=\'table\' AND name=\'t_directorystatus\' ; " ;
	create_query << "CREATE TABLE t_directorystatus " ;
	create_query << "(\"c_configid\" INTEGER NOT NULL , " ;
	create_query << "\"c_contentsrcid\" INTEGER , " ;
	create_query << "\"c_instanceid\" INTEGER  NOT NULL , " ;
	create_query << "\"c_directoryid\" INTEGER PRIMARY KEY AUTOINCREMENT , " ;
	create_query << "\"c_path\" TEXT NOT NULL , " ;
	create_query << "\"c_status\" INTEGER NOT NULL ) ;" ;
	m_con.execQuery(select_query.str(), set) ;
	if( set.getSize() == 0 )
	{
		m_con.execQuery(create_query.str() , set) ;
		DebugPrintf(SV_LOG_DEBUG, "Created the t_directorystatus in icat.db\n") ;
		std::stringstream indexing_query ;
		indexing_query << "CREATE INDEX dirStatusIdx1 ON t_directorystatus ( c_instanceid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;
		indexing_query.str("") ;
		indexing_query << "CREATE INDEX dirStatusIdx2 ON t_directorystatus ( c_configid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;

	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::initFileStatusInfo()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream select_query, create_query ;
	select_query << "	SELECT name FROM sqlite_master WHERE type=\'table\' AND name=\'t_filestatus\' ; " ; 
	create_query << 	"CREATE TABLE t_filestatus " ;
	create_query << 	"(\"c_configid\" INTEGER NOT NULL ,  " ;
	create_query << 	"\"c_instanceid\"  INTEGER NOT NULL , " ;
	create_query << 	"\"c_directoryid\"  INTEGER NOT NULL , " ;
	create_query << 	"\"c_filename\" INTEGER NOT NULL , " ;
	create_query << 	"\"c_modifiedtime\" DATETIME NOT NULL , " ;
	create_query << 	"\"c_checksum\" TEXT NOT NULL , " ;
	create_query << 	"\"c_size\"  INTEGER NOT NULL , " ;
	create_query << 	"\"c_status\" INTEGER NOT NULL ) ; " ;

	m_con.execQuery(select_query.str(), set) ;
	if( set.getSize() == 0 )
	{
		m_con.execQuery(create_query.str(), set)  ;
		DebugPrintf(SV_LOG_DEBUG, "Created the t_filestatus in icat.db\n") ;
		std::stringstream indexing_query ;
		indexing_query << "CREATE INDEX fileStatusIdx1 ON t_filestatus ( c_instanceid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;
		indexing_query.str("") ;
		indexing_query << "CREATE INDEX fileStatusIdx2 ON t_filestatus ( c_configid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;
        

        indexing_query.str("") ;
		indexing_query << "CREATE INDEX fileStatusIdx3 ON t_filestatus ( c_filename ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;


	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

SqliteResumeTracker::SqliteResumeTracker(const std::string & branchName, 
								const std::string& serverName, 
								const std::string& volName,
								const std::string& targetPath,  
								unsigned int maxCopies,
								unsigned int maxDays,
								ICAT_RESUME_TRACKER_MODE trackMode)
: ResumeTracker(branchName, serverName,volName,targetPath, maxCopies, maxDays, trackMode)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::initContentSourceInfo() 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream select_query, create_query ;
	select_query << "SELECT name FROM sqlite_master WHERE type=\'table\' AND name=\'t_contentsource\' ";
	create_query << "CREATE TABLE t_contentsource " ;
	create_query << "(\"c_configid\" INTEGER NOT NULL , " ;
	create_query << "\"c_instanceid\"  INTEGER NOT NULL , " ;
	create_query << "\"c_contentsrc\"  TEXT NOT NULL , " ;
	create_query << "\"c_contentsrcid\" INTEGER PRIMARY KEY AUTOINCREMENT  , " ;
	create_query << "\"c_status\" INTEGER NOT NULL ) ;" ;

	m_con.execQuery(select_query.str() , set) ;
	if( set.getSize() == 0 )
	{
		m_con.execQuery(create_query.str(), set) ;
		DebugPrintf(SV_LOG_DEBUG, "Created the t_contentsource in icat.db\n") ;
		std::stringstream indexing_query ;
		indexing_query << "CREATE INDEX contentSrcIdx1 ON t_contentsource ( c_instanceid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;
		indexing_query.str("") ;
		indexing_query << "CREATE INDEX contentSrcIdx2 ON t_contentsource ( c_configid ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;

	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::initTrackerMode()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_con.open() ;
	initMasterInfo() ;
	initJobStatusInfo() ;
	initContentSourceInfo() ;
	initDirStatusInfo() ;
	initFileStatusInfo() ;
	createConfigId() ;
	createInstanceId() ;
	DebugPrintf(SV_LOG_DEBUG, "Config Id : " ULLSPEC "Instance Id :" ULLSPEC "\n", m_configId, m_instanceId) ;
	setStartTime() ;
	prepareDeleteJobList() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::createConfigId()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
	DBResultSet set ;
	std::stringstream configIdInsertQuery  ;

	configIdInsertQuery << "INSERT INTO t_master (c_remoteoffice , c_servername , c_sourcevolume) " ;
	configIdInsertQuery << " VALUES (\"" << m_branchName << "\" , " ;
	configIdInsertQuery << "\"" << m_serverName << "\" , " ;
	configIdInsertQuery << "\"" << m_volName << "\");" ;
	m_configId = 0 ;
	getConfigId() ;
	if( m_configId == 0 )
	{
		m_con.execQuery(configIdInsertQuery.str(), set) ;
		getConfigId() ;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", __FUNCTION__ ) ;
}

void SqliteResumeTracker::getConfigId()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
	DBResultSet set ;
	std::stringstream configIdSelectQuery , configIdInsertQuery ;
	DBRow row ;
	m_configId = 0 ;
	
	configIdSelectQuery << "SELECT c_configId FROM t_master WHERE" ;
	configIdSelectQuery << " c_remoteoffice = \"" << m_branchName << "\"" ;
	configIdSelectQuery << " AND c_servername = \"" << m_serverName << "\"" ;
	configIdSelectQuery << " AND c_sourcevolume = \"" << m_volName << "\" ; " ;
	m_con.execQuery(configIdSelectQuery.str(), set ) ;
	if( set.getSize() != 0 )
	{
		row = set.nextRow() ;
		m_configId = convertStringtoULL( row.nextVal().getString()) ;
	}
	if( mode() == ICAT_TRACKER_MODE_RESUME && m_configId == 0)
	{
		std::stringstream errorstr ;
		errorstr << "There is no job run in normal mode for branch " ;
		errorstr << m_branchName << " server "  ;
		errorstr << m_serverName ;
		errorstr << " and volume " ;
		errorstr << m_volName << " ; ";
		throw icatException(errorstr.str()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", __FUNCTION__ ) ;
}

void SqliteResumeTracker::createInstanceId()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
	DBResultSet set ;
	std::stringstream instanceIdInsertQuery;

	instanceIdInsertQuery << "INSERT INTO t_jobstatus ( c_configid , c_destinationdirectory, c_starttime, c_endtime, c_status) " ;
	instanceIdInsertQuery << "VALUES ( " ;
	instanceIdInsertQuery << m_configId << " , " ;
	instanceIdInsertQuery << "\"" << m_tgtPath << "\" , " ;
	instanceIdInsertQuery << "\'" << "0000-00-00 00:00:00" << "\' , " ;
	instanceIdInsertQuery << "\'" << "0000-00-00 00:00:00" << "\' , " ;
	instanceIdInsertQuery << 0 << " ) ; " ;
	m_con.execQuery(instanceIdInsertQuery.str(), set) ;
	getInstanceId() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", __FUNCTION__ ) ;
}

void SqliteResumeTracker::getInstanceId()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
	DBResultSet set ;
	DBRow row ;
	std::stringstream instanceIdInsertQuery, instanceIdSelectQuery ;

	instanceIdSelectQuery << "SELECT c_instanceid FROM t_jobstatus WHERE c_configid = " ;
	instanceIdSelectQuery << m_configId << " ORDER BY c_instanceid DESC ; " ;
	
	m_con.execQuery(instanceIdSelectQuery.str(), set) ;
	row = set.nextRow() ;
	m_instanceId  = convertStringtoULL(row.nextVal().getString()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", __FUNCTION__ ) ;
}


void SqliteResumeTracker::initResumeMode()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
    m_con.open() ;
    canRunInResume() ;
    prepareTargetPath() ;
    prepareDeleteJobList() ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
}

void SqliteResumeTracker::canRunInResume()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    DBResultSet set ;
    ConfigValueObject * confObj = ConfigValueObject::getInstance() ;
	std::string filelistFromSource = confObj->getFilelistFromSource();
	m_configId = 0 ;
	getConfigId() ;
	getInstanceId() ;
    if(filelistFromSource.empty() == true)
	{
	    std::list<ContentSource> contentSrcs = confObj->getContentSrcList() ;
    	std::list<std::string> contetsrcdirnamelist ;
    	std::list<ContentSource>::iterator contentSrcIter = contentSrcs.begin() ;

	    while( contentSrcIter != contentSrcs.end() )
    	{
        	ContentSource src = (*contentSrcIter) ;
	        contetsrcdirnamelist.push_back(src.m_szDirectoryName) ;
			contentSrcIter++ ;
    	}
	    std::stringstream query ;
    	query << "SELECT c_contentsrc FROM t_contentsource " ;
	    query << " WHERE c_configid = " << m_configId ;
    	query << " AND c_instanceid = " << m_instanceId << " ; " ;

	    m_con.execQuery( query.str(), set ) ;
	    std::list<std::string>::iterator stringIter = contetsrcdirnamelist.begin() ;

		if( contetsrcdirnamelist.size() != set.getSize() )
    	{	
        	DebugPrintf(SV_LOG_DEBUG, "The number of content sources run in normal mode in last instance and current instances doesnt match..\n") ;
	        DebugPrintf(SV_LOG_DEBUG, "Normal Mode content source directories %d current instance source directories %d\n", set.getSize(), contetsrcdirnamelist.size()) ;
			throw icatException("Can't run in resume mode as the number of content sources differ from normal mode to resume mode\n") ;
    	}
	    else
    	{
        	for( size_t rowIdx = 0 ; rowIdx < set.getSize() ; rowIdx++ )
        	{
            	std::string dirnameNormalMode = set.nextRow().nextVal().getString() ;
				if( find( contetsrcdirnamelist.begin(), contetsrcdirnamelist.end(), dirnameNormalMode ) == contetsrcdirnamelist.end())
        	    {
    	            DebugPrintf(SV_LOG_ERROR, "The directory name in normal mode %s is not found in resume mode directories list\n", dirnameNormalMode.c_str()) ;
					throw icatException("Resume mode can't work as there are config file changes\n") ;
            	}
        	}
    	}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::init()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string dbPath = ConfigValueObject::getInstance()->getLogFilePath() ;
	dbPath.erase(dbPath.length() - 29 ) ;
	dbPath += "_icat.mdb" ;

	m_con.setDbPath(dbPath) ;
	if( this->mode() == ICAT_TRACKER_MODE_RESUME )
	{
		initResumeMode() ;
	}

	if( this->mode() ==  ICAT_TRACKER_MODE_TRACK )
	{
		initTrackerMode() ;
	}
	DebugPrintf(SV_LOG_DEBUG, "The resume db path location is %s\n", dbPath.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::fetchResumeValueObjList(unsigned long long  folderId, ResumeValueObjList & list)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	DBRow row ;
	std::stringstream select_query ;
	select_query << "SELECT c_filename, c_status FROM t_filestatus WHERE " ;
	select_query << " c_configid = " << m_configId ;
	select_query << " AND c_instanceid = " << m_instanceId ;
	select_query << " AND c_directoryid = " << folderId << " ; " ;

	m_con.execQuery(select_query.str(), set ) ;
	list.clear() ;

	for(size_t rowIndex = 0 ; rowIndex < set.getSize() ; rowIndex ++ )
	{
		row = set.nextRow() ;
		ResumeValueObject obj ;
		obj.m_fileName = row.nextVal().getString() ;
		obj.m_status = (ICAT_RESUME_OBJECT_STATUS) ACE_OS::atoi(row.nextVal().getString().c_str()) ;
		list.push_back( obj ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

bool SqliteResumeTracker::shouldtryForResume(unsigned long long folderId, std::string fileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::stringstream select_query ;
	DBResultSet set ;
	bool bRet = true ;
	select_query << "SELECT t_filestatus.c_status FROM t_filestatus " ;
	select_query << " WHERE t_filestatus.c_directoryid = " << folderId ;
	select_query << " AND t_filestatus.c_filename = \"" << fileName << "\" ;" ;;
	m_con.execQuery( select_query.str(), set ) ;
	if( set.getSize() != 0 )
	{
		ICAT_RESUME_OBJECT_STATUS status = (ICAT_RESUME_OBJECT_STATUS) ACE_OS::atoi(set.nextRow().nextVal().getString().c_str()) ;
		if( status != ICAT_RESOBJ_SUCCESS )
		{
			bRet = true ;
		}
		else
		{
			bRet  = false ;
		}
	}
	else
	{
		bRet = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bRet ;
}
void SqliteResumeTracker::addDirectoryStatus(std::string dirName, std::string contentSrcName, unsigned long long contentSrcId, unsigned long long& dirId, ICAT_RESUME_OBJECT_STATUS  status)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	dirId = 0 ;
	DBRow row ;
	std::string relDirPath ;
	std::stringstream insert_query, select_query ;

	relDirPath = dirName.substr(contentSrcName.length() ) ;
	relDirPath.erase( relDirPath.find_last_not_of("/") + 1) ; 
	relDirPath.erase(0 , relDirPath.find_first_not_of("/") ) ;
	relDirPath.erase( relDirPath.find_last_not_of("\\") + 1) ; 
	relDirPath.erase(0 , relDirPath.find_first_not_of("\\") ) ;

	set.clear() ;
	select_query.str("") ;
	select_query << "SELECT c_directoryid , c_status FROM t_directorystatus " ;
	select_query << "WHERE c_configid = " << m_configId ;
	select_query << " AND c_instanceid = " << m_instanceId ;
	select_query << " AND c_contentsrcid = " << contentSrcId ;
	select_query << " AND c_path = \"" << relDirPath.c_str() << "\" ; " ;
	
	insert_query << "INSERT INTO t_directorystatus " ;
	insert_query << " ( c_configid , c_contentsrcid , c_instanceid , c_path , c_status ) " ;
	insert_query << " VALUES ( " << m_configId << " , " ;
	insert_query << contentSrcId << " , " ;
	insert_query << m_instanceId << " , " ;
	insert_query << "\"" << relDirPath << "\" , " ;
	insert_query << status << " ) ; " ;

	if( mode() == ICAT_TRACKER_MODE_RESUME )
	{
		m_con.execQuery( select_query.str(), set ) ;
		if( set.getSize() == 0 )
		{
			if( mode() == ICAT_TRACKER_MODE_RESUME )
			{
				DebugPrintf(SV_LOG_DEBUG, "The directory doesnt exist in normal mode.. Adding it now\n") ;
			}
			m_con.execQuery(insert_query.str(), set ) ;
		}
		else
		{
			DBRow row = set.nextRow() ;
			unsigned long long folderId ;
			folderId = convertStringtoULL(row.nextVal().getString()) ;
			ICAT_RESUME_OBJECT_STATUS status = ( ICAT_RESUME_OBJECT_STATUS ) ACE_OS::atoi(row.nextVal().getString().c_str()) ;
            /* 
			if( status == ICAT_RESOBJ_STAT_FAILED )
			{
				removeFromFileStatus(folderId, dirName ) ;
			}
            */
		}
	}
	else
	{
		m_con.execQuery(insert_query.str(), set ) ;
	}
	m_con.execQuery(select_query.str(), set ) ;
	row = set.nextRow() ;
	dirId = convertStringtoULL(row.nextVal().getString()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
void SqliteResumeTracker::addDirectoryStatus(std::string dirName, unsigned long long contentsrcId, unsigned long long& dirId, ICAT_RESUME_OBJECT_STATUS  status)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	dirId = 0 ;
	DBRow row ;
	std::string contentSrcName ;
	std::stringstream select_query;

	select_query << "SELECT c_contentsrc FROM t_contentsource " ;
	select_query << " WHERE c_configid = " << m_configId ;
	select_query << " AND c_instanceid = " << m_instanceId ;
	select_query << " AND c_contentsrcid = " << contentsrcId << " ; " ;

	m_con.execQuery( select_query.str(), set ) ;
	row = set.nextRow() ;
	contentSrcName = row.nextVal().getString() ;
	addDirectoryStatus(dirName, contentSrcName, contentsrcId, dirId, status) ;

	DebugPrintf(SV_LOG_DEBUG, "Content Source Id: "ULLSPEC" Folder Name : %s Folder Id: "ULLSPEC" \n", contentsrcId, dirName.c_str(), dirId) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::addContentSrcStatus(const std::string &contentSrcName, unsigned long long& contentSrcId, ICAT_RESUME_OBJECT_STATUS status)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	DBRow row ;
	std::stringstream insert_query , select_query ;

	contentSrcId = 0 ;
	
	select_query << "SELECT c_contentsrcid FROM t_contentsource " ;
	select_query << "WHERE c_configid = " << m_configId ;
	select_query << " AND c_instanceid = " << m_instanceId ;
	select_query << " AND c_contentsrc = \"" << contentSrcName << "\" ; " ;

	insert_query << "INSERT INTO t_contentsource " ;
	insert_query << "( c_configid , c_instanceid , c_contentsrc , c_status ) " ;
	insert_query << " VALUES ( " << m_configId << " , " ;
	insert_query << m_instanceId << " , " ;
	insert_query << "\""<< contentSrcName << "\" , " ;
	insert_query << status << " ) ; " ;
	if( mode() == ICAT_TRACKER_MODE_RESUME )
	{
		m_con.execQuery( select_query.str(), set ) ;
		if( set.getSize() == 0 )
		{
			if( mode() == ICAT_TRACKER_MODE_RESUME )
			{
				DebugPrintf(SV_LOG_DEBUG, "The directory doesnt exist in normal mode.. Adding it now\n") ;
			}
			m_con.execQuery(insert_query.str(), set ) ;
		}
	}
	else
	{
		m_con.execQuery(select_query.str(), set ) ;
		 if( set.getSize() == 0 )
        {
            m_con.execQuery(insert_query.str(), set ) ;
        }

	}
	m_con.execQuery(select_query.str(), set ) ;
	row = set.nextRow() ;
	contentSrcId  = convertStringtoULL(row.nextVal().getString()) ;
	m_contentSrcIdMap.insert(std::pair<unsigned long long, std::string>(contentSrcId, contentSrcName)) ;
	DebugPrintf(SV_LOG_DEBUG, "Content Source Name :%s  Content Source Id:"  ULLSPEC "\n", contentSrcName.c_str(), contentSrcId) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::updateFileStatus(const std::string& fileName, const ACE_stat& stat, unsigned long long dirId, ICAT_RESUME_OBJECT_STATUS status )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	DBRow row ;
	std::string fileModTime ;
	unsigned long long filesize = 0 ;
	if( status == ICAT_RESOBJ_STAT_FAILED )
	{
		fileModTime = "0000-00-00 00:00:00" ;
		filesize  = 0 ;
	}
	else
	{
		getTimeToString("%Y-%m-%d %H:%M:%S" , fileModTime, (time_t*)&stat.st_mtime) ;
		filesize = stat.st_size ;
	}
	std::stringstream insert_query, update_query, select_query ;
	insert_query << "INSERT INTO t_filestatus " ;
	insert_query << " ( c_configid, c_instanceid , c_directoryid , c_filename , c_modifiedtime , c_checksum , c_size , c_status ) " ;
	insert_query << " VALUES ( " << m_configId << " , " ;
	insert_query <<  m_instanceId << " , " ;
	insert_query <<  dirId << " , " ;
	insert_query << "\"" << fileName << "\" , " ;
	insert_query << "\"" << fileModTime << "\" , " ;
	insert_query << "\"" << "CHECKSUM" << "\" , " ;
	insert_query << filesize << " , " ;
	insert_query << status << " ) ; " ;

	if( mode() == ICAT_TRACKER_MODE_TRACK )
	{
		m_con.execQuery( insert_query.str(), set ) ;
	}
	else
	{
		select_query << "SELECT c_status FROM t_filestatus " ;
		select_query << " WHERE c_directoryid = " << dirId ;
		select_query << " AND c_filename = \"" << fileName << "\" ; ";
		m_con.execQuery(select_query.str(), set) ;
		if( set.getSize() == 0 )
		{
			m_con.execQuery( insert_query.str(), set ) ;
		}
		m_con.execQuery(select_query.str(), set) ;
		ICAT_RESUME_OBJECT_STATUS previousFileStatus = (ICAT_RESUME_OBJECT_STATUS) ACE_OS::atoi(set.nextRow().nextVal().getString().c_str()) ;
		if( previousFileStatus == ICAT_RESOBJ_STAT_FAILED )
		{
			removeFromFolderStatus(dirId) ;
		}
		update_query << "UPDATE t_filestatus " ;
		update_query << " SET c_status = " << status ;
		update_query << " WHERE c_directoryid = " << dirId ;
		update_query << " AND c_filename = \"" << fileName << "\" ; ";
		m_con.execQuery( update_query.str(), set ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "FileName :%s Directory Id:"  ULLSPEC " \n", fileName.c_str(), dirId) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::setStartTime()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream  query ;

	if( mode() == ICAT_TRACKER_MODE_RESUME )
	{
		query.str("") ;
		query << "UPDATE t_jobstatus ";
		query << " SET c_endtime = " << "\"0000-00-00 00:00:00\"" ;
		query << " WHERE c_configid = " << m_configId ;
		query << " AND c_instanceid = " << m_instanceId ;
		query << " ; " ;
		m_con.execQuery(query.str(), set) ;
	}
	else
	{
		query.str("") ;
		query << "UPDATE t_jobstatus SET c_starttime = " ;
		query << "\'" << archiveController::getInstance()->getStartTime("%Y-%m-%d %H:%M:%S") << "\'" ;
		query << " WHERE c_configid = " << m_configId ;
		query << " AND c_instanceid = " << m_instanceId ;
		query << " ; " ;

		m_con.execQuery(query.str(), set) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Start Time %s\n", archiveController::getInstance()->getStartTime("%Y-%m-%d %H:%M:%S").c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::setEndTime()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream  query ; 
	std::string timeStr = "0000-00-00 00:00:00" ;
	finalize() ;
	getTimeToString("%Y-%m-%d %H:%M:%S" , timeStr ) ;
	query << "UPDATE t_jobstatus SET c_endtime = " ;
	query << "\'" << timeStr << "\'" ;
	query << " WHERE c_configid = " << m_configId ;
	query << " AND c_instanceid = " << m_instanceId ;
	query << " ; " ;
	m_con.execQuery(query.str(), set) ;
	DebugPrintf(SV_LOG_DEBUG, "The End Time %s\n", timeStr.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::updateFolderStatus(unsigned long long folderId, ICAT_RESUME_OBJECT_STATUS status)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	unsigned long long contentSrcId = 0 ;
	DBRow row ;
	std::stringstream update_query ;

	update_query << "UPDATE t_directorystatus " ;
	update_query << " SET c_status = " << status ;
	update_query << " WHERE c_directoryid = " <<	folderId << " ; " ;

	m_con.execQuery(update_query.str(), set) ;
	if( status != ICAT_RESOBJ_SUCCESS )
	{
		markContentSourceByFolderId(folderId, status) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::markContentSourceByFolderId(unsigned long long folderId, ICAT_RESUME_OBJECT_STATUS status)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	unsigned long long contentSrcId = 0 ;
	DBRow row ;
	std::stringstream update_query ;

	update_query.str("") ;
	update_query << "SELECT c_contentsrcid FROM t_directorystatus " ;
	update_query << " WHERE c_directoryid = " << folderId  << " ; " ;
	m_con.execQuery(update_query.str(), set) ;
	row = set.nextRow() ;
	contentSrcId = convertStringtoULL(row.nextVal().getString()) ;
	markContentSourceStatus(contentSrcId, status) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::markContentSourceStatus(unsigned long long contentsrcId, ICAT_RESUME_OBJECT_STATUS status)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	DBRow row ;
	std::stringstream update_query ;
	update_query << "UPDATE t_contentsource " ;
	update_query << " SET c_status = " << status ;
	update_query << " WHERE c_contentsrcid = " << contentsrcId << " ; " ;
	m_con.execQuery(update_query.str(), set) ; 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

std::string SqliteResumeTracker::getNormalModeBasePath()
{
	return m_tgtPath ;
}

void SqliteResumeTracker::prepareTargetPath()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	DBRow row ;
	std::stringstream select_query ;
	select_query << "SELECT c_destinationdirectory FROM t_jobstatus " ;
	select_query << "WHERE c_instanceid = " << m_instanceId ;
	select_query << " AND c_configid = " << m_configId << " ; " ;
	m_con.execQuery(select_query.str(), set) ;
	row = set.nextRow() ;
	m_tgtPath = row.nextVal().getString() ;
	if( ConfigValueObject::getInstance()->getTransport().compare("http") == 0 ||
		ConfigValueObject::getInstance()->getTransport().compare("nfs") == 0 ) 
	{
		for (unsigned i=0; i < m_tgtPath.length(); ++i) if (m_tgtPath[i]=='\\') m_tgtPath[i]='/';
	}
	else
	{
		for (unsigned i=0; i < m_tgtPath.length(); ++i) if (m_tgtPath[i]=='/') m_tgtPath[i]='\\';
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

bool SqliteResumeTracker::isResumeRequired(unsigned long long contentSrcId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	DBRow row ;
	bool resumeRequired = true ;
	std::stringstream select_query ;
	select_query << "SELECT c_status FROM t_contentsource " ;
	select_query << "WHERE c_configid = " << m_configId ;
	select_query << " AND c_instanceid = " << m_instanceId ;
	select_query << " AND c_contentsrcid = " << contentSrcId  ;
	select_query << " AND c_status != " << ICAT_RESOBJ_SUCCESS << " ; " ;
	m_con.execQuery( select_query.str(), set ) ;
	if(set.getSize() == 0 )
	{
		resumeRequired = false ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXTED %s\n", __FUNCTION__) ;
	return resumeRequired ;
}

void SqliteResumeTracker::finalize()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::stringstream query ;
	DBResultSet set ;
	
	query << "SELECT DISTINCT c_directoryid FROM t_filestatus " ;
	query << " WHERE c_configid = " << m_configId ;
	query << " AND c_instanceid = " << m_instanceId ;
	query << " AND c_status != " << ICAT_RESOBJ_SUCCESS << " ; " ; 
	m_con.execQuery(query.str(), set ) ;
	
	std::string directoryIds ="" ;
	std::string contentsrcIds = "" ;
	if( set.getSize() != 0 )
	{
		for(size_t rowIdx = 0 ; rowIdx < set.getSize() ; rowIdx++)
		{
			directoryIds += set.nextRow().nextVal().getString() ;
			directoryIds += "," ;
		}
		directoryIds.erase(directoryIds.length() - 1 ) ;
		query.str("") ;
		query << "SELECT DISTINCT c_contentsrcid FROM t_directorystatus " ;
		query << "WHERE c_directoryid IN ( " << directoryIds << " )  GROUP BY c_contentsrcid ; " ;
		m_con.execQuery(query.str(), set) ;
		if( set.getSize() != 0 )
		{
			for(size_t rowIdx = 0 ; rowIdx < set.getSize() ; rowIdx++)
			{
				contentsrcIds += set.nextRow().nextVal().getString() ;
				contentsrcIds += "," ;
			}
			contentsrcIds.erase(contentsrcIds.length() - 1 ) ;
			query.str("") ;
			query << "UPDATE t_contentsource set c_status = " << ICAT_RESOBJ_FAILURE ;
			query << " WHERE  c_contentsrcid IN ( " << contentsrcIds << " ) ; " ;
			m_con.execQuery(query.str(), set) ;
		}
	}
	query.str("") ;
	query << "UPDATE t_contentsource set c_status = " << ICAT_RESOBJ_SUCCESS ;
	query << " WHERE  c_configid = " << m_configId ;
	query << " AND c_instanceid = " << m_instanceId ;
	query << " AND c_contentsrcid NOT IN ( " << contentsrcIds << " ) " ; 
	query << " AND c_status != " << ICAT_RESOBJ_FAILURE << " ; " ;
	m_con.execQuery(query.str(), set) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::getLastRunTime(std::string& lastRunTime)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::stringstream query ;
	DBResultSet set ;
	DBRow row ;
	if( m_lastRunTime.compare("") == 0 )
	{
		m_lastRunTime = "0000-00-00 00:00:00" ;
		query << "SELECT c_starttime FROM t_jobstatus " ;
		query << " WHERE c_configid = " << m_configId ;
		query << " AND c_endtime != '0000-00-00 00:00:00'" ;
		query << " ORDER BY c_starttime DESC ; " ; 

		try
		{
			m_con.execQuery(query.str(), set) ;
		}
		catch(icatException &e)
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to find last run time. %s\n", e.what()) ;
		}
		if( set.getSize() != 0 )
		{
			m_lastRunTime = set.nextRow().nextVal().getString() ;
		}
		DebugPrintf(SV_LOG_DEBUG, "Last Instance Start Time : %s\n", m_lastRunTime.c_str()) ;
	}
	lastRunTime = m_lastRunTime ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
}



void SqliteResumeTracker::prepareDeleteJobList()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string c_destinationDirs = "" ;
    std::stringstream query ;
    DBResultSet set ;
    if( m_maxDays != 0 )
    {
        std::list<JobStatus> jobStatusList ;
        std::string timeStrNow ;
        getTimeToString("%Y-%m-%d %H:%M:%S" , timeStrNow, NULL) ;
        
        query << "SELECT t_jobstatus.c_instanceid, t_jobstatus.c_destinationdirectory " ;
		query << "FROM t_jobstatus " ;
        query << " WHERE strftime(\'%s\', \'"<< timeStrNow << "\') - strftime(\'%s\' , t_jobstatus.c_starttime) > " << m_maxDays * 60 * 60 * 24 ;
        query << " AND c_endtime != '0000-00-00 00:00:00' AND c_instanceid != " << m_instanceId << ";";

		if( set.getSize() > 0 )
		{
			DebugPrintf(SV_LOG_DEBUG, "Following are the instances needs to be deleted based on maxDays condition\n") ;
		}
        m_con.execQuery(query.str(), set) ;

        char fromChar, toChar ;
        if( ConfigValueObject::getInstance()->getTransport().compare("http") == 0 ||
            ConfigValueObject::getInstance()->getTransport().compare("nfs") == 0 )
        {
            fromChar = '\\' ;
            toChar = '/' ;
        }
        else
        {
            fromChar = '/' ;
            toChar = '\\' ;
        }

        for( size_t rowIdx = 0 ; rowIdx < set.getSize(); rowIdx ++ )
        {
            DBRow row = set.nextRow() ;
			unsigned long long instanceid = convertStringtoULL(row.nextVal().getString()) ;
			std::string destPath = row.nextVal().getString() ;
                
            for (unsigned i=0; i<destPath.length(); ++i) 
            {
                if ( destPath[i] == fromChar ) 
                        destPath[i] = toChar ;
            }

			query.str("") ;
			query << "SELECT c_contentsrcid, c_contentsrc FROM t_contentsource " ;
			query << " WHERE c_instanceid = " << instanceid ;
			DBResultSet set1 ;
			m_con.execQuery(query.str(), set1) ;

			for( size_t rowIdx1 = 0 ; rowIdx1 < set1.getSize() ;rowIdx1 ++ )
			{
				DBRow row1 = set1.nextRow() ;
				JobStatus jobStatus ;
				jobStatus.m_instanceId = instanceid ;
				jobStatus.m_destinationPath = destPath ;
				jobStatus.m_contentsrcId = convertStringtoULL(row1.nextVal().getString()) ;
				jobStatus.m_contentsrcName = row1.nextVal().getString() ;

				c_destinationDirs += "'";
				c_destinationDirs += jobStatus.m_destinationPath ;
				c_destinationDirs += "'";
				c_destinationDirs += "," ;
				DebugPrintf(SV_LOG_DEBUG, "Job Instance Id : " ULLSPEC " Content Src Id :" ULLSPEC "\n", jobStatus.m_instanceId, jobStatus.m_contentsrcId) ;
				m_deleteList.push_back(jobStatus) ;
			}
        }
        if(c_destinationDirs.compare("") != 0 )
        {
            c_destinationDirs.erase(c_destinationDirs.length() -1 ) ;
        }
    }

    if( m_maxCopies != 0 )
    {
        query.str("") ;
        query << "SELECT c_instanceid, c_destinationdirectory FROM t_jobstatus " ;
        if( c_destinationDirs.compare("") != 0 )
        {
            query << " WHERE c_destinationdirectory NOT IN ( " << c_destinationDirs << ") AND " ;
			query << " c_endtime != \"0000-00-00 00:00:00\" " ;
	        query << " ORDER BY c_starttime DESC ;" ;
        }
		else
		{
			query << " WHERE c_endtime != \"0000-00-00 00:00:00\" AND c_instanceid != " << m_instanceId ;
	        query << " ORDER BY c_starttime DESC ;" ;
		}
        m_con.execQuery(query.str(), set) ;
        if( set.getSize() >= m_maxCopies )
        {
            for( unsigned int i = 0 ;i < m_maxCopies - 1; i++)
            {
                set.nextRow() ;
            }
        }

		if( set.getSize() >= (size_t) m_maxCopies )
        {
			DebugPrintf(SV_LOG_DEBUG, "Following are the instances needs to be deleted based on maxCopies condition\n") ;
            for( size_t rowIdx = 0 ;rowIdx < ( set.getSize() - ( (size_t) (m_maxCopies - 1)) ); rowIdx++)
            {
				DBRow row = set.nextRow() ;
				unsigned long long instanceid = convertStringtoULL(row.nextVal().getString()) ;
				std::string destPath = row.nextVal().getString() ;
				query.str("") ;
				query << "SELECT c_contentsrcid, c_contentsrc FROM t_contentsource " ;
				query << " WHERE c_instanceid = " << instanceid ;
				DBResultSet set1 ;
				m_con.execQuery(query.str(), set1) ;
				for( size_t rowIdx1 = 0 ; rowIdx1 < set1.getSize() ;rowIdx1 ++ )
				{
					JobStatus jobStatus ;
					DBRow row1 = set1.nextRow() ;
					jobStatus.m_instanceId = instanceid ;
					jobStatus.m_contentsrcId = convertStringtoULL(row1.nextVal().getString()) ;
					jobStatus.m_destinationPath = destPath ;
                    jobStatus.m_contentsrcName = row1.nextVal().getString() ;

					DebugPrintf(SV_LOG_DEBUG, "Job Instance Id : " ULLSPEC " Content Src Id :" ULLSPEC "\n", jobStatus.m_instanceId, jobStatus.m_contentsrcId) ;
					m_deleteList.push_back(jobStatus) ;
				}
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::getDeleteableFiles(std::list<ResumeTrackerObj>& deleteList, int size)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::list<JobStatus>::iterator begin = m_deleteList.begin() ;
	std::stringstream query ;
	DBResultSet set ;
	while( begin != m_deleteList.end() && 
        deleteList.size() < size )
	{
		JobStatus jobstatus = (*begin) ;
		query.str("") ;
		query << "SELECT " ;
		query << "t_directorystatus.c_directoryid , " ;
		query << "t_directorystatus.c_path , " ;
		query << "t_filestatus.c_filename , " ;
		query << "t_filestatus.c_status " ;
		query << "FROM  " ;
		query << "t_directorystatus , " ;
		query << "t_filestatus "   ;
		query << "WHERE  " ;
		query << "t_directorystatus.c_contentsrcid = " << jobstatus.m_contentsrcId ;
		query << " AND t_directorystatus.c_directoryid = t_filestatus.c_directoryid " ;
		query << " AND ( t_filestatus.c_status != "<< ICAT_RESOBJ_PENDINGDEL ;
		query << " AND t_filestatus.c_status = "<< ICAT_RESOBJ_SUCCESS << " ) ";
		query << " LIMIT " << size - (int) deleteList.size() << " ; " ;

		m_con.execQuery(query.str(), set) ;
		for(size_t rowIdx = 0 ; rowIdx < set.getSize() ; rowIdx++ )
		{
			DBRow row = set.nextRow();
			ResumeTrackerObj obj ;
			obj.m_configId = jobstatus.m_configId ;
			obj.m_instanceId = jobstatus.m_instanceId ;
			obj.m_contentSrcId = jobstatus.m_contentsrcId ;
			obj.m_contentSrcPath = jobstatus.m_contentsrcName ;
			obj.m_tgtPath = jobstatus.m_destinationPath ;

			obj.m_dirId = convertStringtoULL(row.nextVal().getString()) ;
			obj.m_dirPath = row.nextVal().getString() ;
			obj.m_fileName = row.nextVal().getString() ;
			obj.m_status = (ICAT_RESUME_OBJECT_STATUS) ACE_OS::atoi(row.nextVal().getString().c_str()) ;
			obj.m_isDir = false ;
			deleteList.push_back(obj) ;		
		}
		begin++ ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::getDeleteableDirs(std::list<ResumeTrackerObj>& deleteList, int size)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::list<JobStatus>::iterator begin = m_deleteList.begin() ;
	std::stringstream query ;
	DBResultSet set ;
	while( begin != m_deleteList.end() )
	{
		if( (int ) deleteList.size() >= size )
		{
			break ;
		}
		JobStatus jobstatus = (*begin) ;
		query.str("") ;
		query << "SELECT " ;
		query << "t_directorystatus.c_directoryid , " ;
		query << "t_directorystatus.c_path , " ;
		query << "t_directorystatus.c_status " ;
		query << "FROM  " ;
		query << "t_directorystatus " ;
		query << "WHERE  " ;
		query << "t_directorystatus.c_instanceid = " << jobstatus.m_instanceId ;
		query << " AND t_directorystatus.c_contentsrcid = " << jobstatus.m_contentsrcId ;
		query << " AND  ( t_directorystatus.c_status != "<< ICAT_RESOBJ_CANTDELETE_FOLDER ;
		query << " AND t_directorystatus.c_status != "<< ICAT_RESOBJ_PENDINGDEL << " ) " ;
		query << " ORDER BY c_directoryid DESC " << " " ;
		query << " LIMIT " << size - (int) deleteList.size() << " ; " ;
		m_con.execQuery(query.str(), set) ;

		for(size_t rowIdx = 0 ; rowIdx < set.getSize() ; rowIdx++ )
		{
			if( (int ) deleteList.size() >= size )
			{
				break ;
			}
			DBRow row = set.nextRow();
			ResumeTrackerObj obj ;
			obj.m_configId = jobstatus.m_configId ;
			obj.m_instanceId = jobstatus.m_instanceId ;
			obj.m_contentSrcId = jobstatus.m_contentsrcId ;
			obj.m_contentSrcPath = jobstatus.m_contentsrcName ;
			obj.m_fileName = "" ;
			obj.m_tgtPath = jobstatus.m_destinationPath ;

			obj.m_dirId = convertStringtoULL(row.nextVal().getString()) ;
			obj.m_dirPath = row.nextVal().getString() ;
			obj.m_status = (ICAT_RESUME_OBJECT_STATUS) ACE_OS::atoi(row.nextVal().getString().c_str()) ;
			obj.m_isDir = true ;
			deleteList.push_back(obj) ;		
		}
		begin++ ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::getDeleteableObjects(std::list<ResumeTrackerObj>& deleteList, int size)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	deleteList.clear() ;
	getDeleteableFiles(deleteList, size) ;
	if( deleteList.size() == 0 )
	{
		getDeleteableDirs(deleteList, size) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::updateDeleteStatus(const std::string& name, unsigned long long folderId , bool isDir, ICAT_RESUME_OBJECT_STATUS status)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
	std::stringstream query ;
	DBResultSet set ;
	if( !isDir )
	{
		if( status == ICAT_RESOBJ_DELETED )
		{
			query.str("") ;
			query << "DELETE FROM t_filestatus WHERE " ;
			query << "c_directoryid = " << folderId ;
			query << " AND c_filename = \"" << name << "\" ; ";
			m_con.execQuery(query.str(), set) ;
		}
		else
		{
			query.str("") ;
			query << "UPDATE t_filestatus SET c_status = " << status ;
			query << " WHERE c_directoryid = " << folderId ;
			query << " AND c_filename = \"" << name << "\" ; ";
			m_con.execQuery(query.str(), set) ;
			if( status == ICAT_RESOBJ_DELETE_FAILED )
			{
				query.str("") ;
				query << "UPDATE t_directorystatus " ;
				query << "SET c_status = " << status  ;
				query << " WHERE c_directoryid = " << folderId << " ; " ;
				m_con.execQuery(query.str(), set) ;
			}
		}
	}
	else
	{
		if( status == ICAT_RESOBJ_DELETED )
		{
			query.str("") ;
			unsigned long long instanceid = 0 ;
			if( name.compare("") == 0 )
			{
				query << "SELECT c_instanceid FROM t_directorystatus " ;
				query << "WHERE c_directoryid = " << folderId << " ;" ;
				m_con.execQuery(query.str(), set) ;
				instanceid = convertStringtoULL(set.nextRow().nextVal().getString()) ;
			}
			query.str("") ;
			query << "DELETE FROM t_directorystatus  " ;
			query << " WHERE  c_directoryid = " << folderId << " ; " ;
			m_con.execQuery(query.str(), set) ;
			if( instanceid != 0 )
			{
				query.str("") ;
				query << "DELETE FROM t_jobstatus " ;
				query << " WHERE c_instanceid = " << instanceid << " ; " ;
				m_con.execQuery(query.str(), set) ;
			}
			
		}
		else
		{
			query.str("") ;
			query << "UPDATE t_directorystatus " ;
			query << "SET c_status = " << status  ;
			query << " WHERE c_directoryid = " << folderId << " ; " ;
			m_con.execQuery(query.str(), set) ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteResumeTracker::addDirectoryAndFileStatus(std::string absPath, unsigned long long contentSrcId, ICAT_RESUME_OBJECT_STATUS  status) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ );
	ACE_stat stat ;
	unsigned long long dirId = 0 ;
	addDirectoryStatus(absPath, contentSrcId, dirId, status) ;
	updateFileStatus(absPath.substr(absPath.find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR_A) + 1 ), stat, dirId, status) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ );
}

void SqliteResumeTracker::removeFromFileStatus(unsigned long long folderId, std::string fileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ );
	std::stringstream query ;
	DBResultSet set ;
	query << "DELETE FROM t_filestatus " ;
	query << " WHERE c_directoryid = " << folderId ;
	query << " AND c_filename = \"" << fileName << "\" ; " ;
	m_con.execQuery(query.str(), set ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ );
}

void SqliteResumeTracker::removeFromFolderStatus(unsigned long long folderId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ );
	std::stringstream query ;
	DBResultSet set ;
	query << "DELETE FROM t_directorystatus " ;
	query << " WHERE c_directoryid = " << folderId << " ;" ;
	m_con.execQuery(query.str(), set ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ );
}
void SqliteResumeTracker::markDeletePendingToSuccess()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ );
    
    std::stringstream update_query ;
    DBResultSet set ;
    update_query << "UPDATE t_filestatus " ;
	update_query << "SET c_status = " << ICAT_RESOBJ_SUCCESS ;
	update_query << " WHERE c_status = " << ICAT_RESOBJ_PENDINGDEL ;
    update_query << " OR c_status = " << ICAT_RESOBJ_DELETE_FAILED << " ;" ;
	m_con.execQuery( update_query.str(), set ) ;
    DebugPrintf(SV_LOG_DEBUG, "No of updated rows in t_filestatus from deletete pending to success are %d \n", set.getSize() ) ;

    std::stringstream update_dir_query ;
    update_dir_query << "UPDATE t_directorystatus " ;
	update_dir_query << "SET c_status = " << ICAT_RESOBJ_SUCCESS ;
	update_dir_query << " WHERE c_status = " << ICAT_RESOBJ_PENDINGDEL ;
    update_dir_query << " OR c_status = " << ICAT_RESOBJ_DELETE_FAILED << " ;" ;
	m_con.execQuery( update_dir_query.str(), set ) ;
    DebugPrintf(SV_LOG_DEBUG, "No of updated rows in t_directorystatus from deletete pending to success are %d \n", set.getSize() ) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ );
}
