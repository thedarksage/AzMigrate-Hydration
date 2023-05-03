#include "confschemareaderwriter.h"
#include "alertconfig.h"
#include "licenseconfig.h"
#include "policyconfig.h"
#include "protectionpairconfig.h"
#include "scenarioconfig.h"
#include "volumeconfig.h"
#include "inmstrcmp.h"
#include "confschemamgr.h"
#include "inmsdkutil.h"
#include "apinames.h"
#include <boost/property_tree/detail/xml_parser_error.hpp>
#include <boost/property_tree/detail/rapidxml.hpp>
#include <boost/filesystem/operations.hpp>
#include "APIController.h"

ConfSchemaReaderWriterPtr ConfSchemaReaderWriter::m_ConfSchemaReaderWriterPtr ;

void ConfSchemaReaderWriter::CreateInstance(const std::string& apiname, 
											const std::string& repoLocation, 
											const std::string& repositoryPathForHost,
											bool bProcessEvenWithoutlock)
{
	m_ConfSchemaReaderWriterPtr.reset( new ConfSchemaReaderWriter( repoLocation, repositoryPathForHost ) ) ;
	if( !m_ConfSchemaReaderWriterPtr )
	{
		throw "Failed to create the confschema readerwriter" ;
	}
	if( apiname.length() > 0 )
	{
		if( !repositoryPathForHost.empty() )
		{
			SVMakeSureDirectoryPathExists(repositoryPathForHost.c_str()) ;
			if( !bProcessEvenWithoutlock )
			{
				m_ConfSchemaReaderWriterPtr->RollBackTransaction() ;
			}
		}
	}
	if( !apiname.empty() )
	{
		bool IsRecoverablecorruption = ActOnCorruptedGroups() ;
		if( !IsRecoverablecorruption )
		{
			if( !(apiname.compare( "ReconstructRepo") == 0 || 
				apiname.compare( "DownloadAlerts" ) == 0 || 
				apiname.compare( "ShouldMoveOrArchiveRepository" ) == 0 ||
				apiname.compare( "ListConfiguredRepositories" ) == 0 ||
				apiname.compare( "DeleteRepository" ) == 0 ||
				apiname.compare( "HealthStatus" ) == 0 ) )
			{
				throw TransactionException("Configuration store corrupted", E_SYSTEMBUSY ) ;
			}
		}
	}
	return;
}

ConfSchemaReaderWriterPtr ConfSchemaReaderWriter::GetInstance()
{
	if( !m_ConfSchemaReaderWriterPtr )
	{
		throw "GetInstance is called before calling CreateInstance" ;
	}
	return m_ConfSchemaReaderWriterPtr ;
}

ConfSchemaReaderWriter::ConfSchemaReaderWriter(const std::string& repoLocation, const std::string& repositoryPathForHost )
//:m_bkpmgr(".save",configdir )
{
	std::string configdir ;
	configdir = repositoryPathForHost ;
	if( repositoryPathForHost.length() > 0 && 
		repositoryPathForHost[repositoryPathForHost.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR )
	{
		configdir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	}
	configdir += "configstore" ;
	DebugPrintf(SV_LOG_DEBUG, "ConfSchemaReaderWriter repo location %s, repopath for host %s, configdir %s\n", 
		repoLocation.c_str(), repositoryPathForHost.c_str(), configdir.c_str()) ;
	m_bkpmgr.reset(new InmFileBackupManager(".save", configdir)) ;
	schemaMgrPtr = ConfSchemaMgr::CreateInstance(repoLocation,  repositoryPathForHost ) ;
}

ConfSchema::Group* ConfSchemaReaderWriter::getGroup(const std::string& grpname)
{
	ConfSchemaMgrPtr confSchemaMgrPtr = ConfSchemaMgr::GetInstance() ;
	ConfSchema::Groups_t &groups = confSchemaMgrPtr->getGroups() ;
	ConfSchema::GroupsIter_t groupIter = groups.find(grpname);
	bool corruption = false ;
	bool readfailure = false ;
	std::string errmsgstr ;
	std::string xmlfile ;
	std::string corruptfile ;
	try
	{

		xmlfile = schemaMgrPtr->GetSchemaFilePath( grpname ) ;
		corruptfile = xmlfile + ".corrupt" ;
		if (groups.end() == groupIter)
		{
			DebugPrintf(SV_LOG_DEBUG, "Reading the group %s from %s\n", grpname.c_str(), xmlfile.c_str()) ;
			schemaMgrPtr->ReadConfSchema(grpname) ;
		}
	}
	catch(boost::property_tree::xml_parser::xml_parser_error& xpe)
	{
		errmsgstr = xpe.message().c_str() ;
		DebugPrintf(SV_LOG_ERROR, "%s configuration parse error : %s\n", grpname.c_str(), errmsgstr.c_str()) ;
		if( strcmp( errmsgstr.c_str(), "read error") == 0  || 
			strcmp( errmsgstr.c_str(), "cannot open file") == 0 )
		{
			readfailure = true ;
		}
		else
		{			
			corruption = true ;
		}
	}
	catch( std::exception& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "unable to load the group %s\n", ex.what()) ;
		corruption = true ;
	}
	if( readfailure )
	{
		throw TransactionException(errmsgstr, E_SYSTEMBUSY ) ;
	}
	if( corruption )
	{
		if( boost::filesystem::exists( corruptfile.c_str() ) )
		{
			ACE_OS::unlink( corruptfile.c_str() ) ;
		}
		try
		{
			boost::filesystem::rename( xmlfile.c_str(), corruptfile.c_str() ) ;
		}
		catch( boost::filesystem::basic_filesystem_error<boost::filesystem::path> e )
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to rename the file %s, error %s \n", xmlfile.c_str(), e.what()) ;
		}
		throw TransactionException("Configuration store corrupted", E_SYSTEMBUSY ) ;
	}
	groupIter = groups.find(grpname) ;
	if (groups.end() == groupIter)
	{
		groups.insert(std::make_pair(grpname, ConfSchema::Group()));
	}
	groupIter = groups.find(grpname) ;
	return &(groupIter->second) ;
}

void ConfSchemaReaderWriter::RollBackTransaction()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string transactionLogPath = schemaMgrPtr->getConfigDirLocation() ;
	transactionLogPath += ACE_DIRECTORY_SEPARATOR_CHAR ;
	transactionLogPath += "transaction.log" ;
	size_t idx1 = transactionLogPath.find_last_of( ACE_DIRECTORY_SEPARATOR_CHAR ) ;
	std::string dir = transactionLogPath.substr(0, idx1) ;
	SVMakeSureDirectoryPathExists(dir.c_str()) ;
	try 
	{
		if( boost::filesystem::exists( dir.c_str() ) )
		{
			InmFileBackupManager bkpmgr(".save", dir);
			if (!bkpmgr.GetBackupFiles())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to get backup files with error: %s\n", bkpmgr.GetErrorMsg().c_str());
				throw TransactionException("Failed to get backup files", E_SYSTEMBUSY) ;
			}
			InmLogBasedTransaction pit(transactionLogPath.c_str(), &bkpmgr) ;
			if( !pit.Rollback() )
			{
				DebugPrintf(SV_LOG_ERROR, "rollback of previous transaction failed with error: %s\n", 
					pit.GetErrorMsg().c_str());
				throw TransactionException("rollback of previous transaction failed", E_SYSTEMBUSY) ;
			}
		}
	}
	catch (std::exception &ex )
	{
		DebugPrintf (SV_LOG_ERROR, "%s \n",ex.what());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ConfSchemaReaderWriter::BeginTransaction(boost::shared_ptr<InmTransaction> pit)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	if (!pit->Begin(m_apiname))
	{
		DebugPrintf(SV_LOG_ERROR, "begin transaction failed with %s\n", pit->GetErrorMsg().c_str());
		if (strcmp (strerror(errno),"No such file or directory")== 0 )
		{
			throw TransactionException("", E_REPO_NOT_AVAILABLE) ;
		}
		else
		{
			throw TransactionException(pit->GetErrorMsg().c_str(), E_SYSTEMBUSY) ;
		}
	}
	std::set<std::string>::const_iterator filenameIter ;
	filenameIter = m_filenames.begin() ;
	while( filenameIter != m_filenames.end() )
	{
		std::string item = filenameIter->substr(filenameIter->find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR) + 1) ;
		if (!pit->AddItemToUpdate(item))
		{
			DebugPrintf(SV_LOG_ERROR, "additem failed with %s\n", pit->GetErrorMsg().c_str());
			if (strcmp (strerror(errno),"No such file or directory")== 0 )
			{
				throw TransactionException("", E_REPO_NOT_AVAILABLE) ;
			}
			else
			{
				throw TransactionException(pit->GetErrorMsg().c_str(), E_SYSTEMBUSY) ;
			}
		}
		filenameIter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ConfSchemaReaderWriter::EndTransaction(boost::shared_ptr<InmTransaction> pit)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::set<std::string>::const_iterator filenameIter ;
	filenameIter = m_filenames.begin() ;
	while( filenameIter != m_filenames.end() )
	{
		std::string item = filenameIter->substr(filenameIter->find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR) + 1) ;
		if (!pit->SetItemAsUpdated(item.c_str()))
		{
			DebugPrintf(SV_LOG_ERROR, "SetItemAsUpdated  failed with %s\n", pit->GetErrorMsg().c_str());
			if (strcmp (strerror(errno),"No such file or directory")== 0 )
			{
				throw TransactionException("", E_REPO_NOT_AVAILABLE) ;
			}
			else
			{
				throw TransactionException(pit->GetErrorMsg().c_str(), E_SYSTEMBUSY) ;
			}
		}
		filenameIter++ ;
	}
	if (!pit->End())
	{
		DebugPrintf(SV_LOG_ERROR, "end failed with error %s\n", pit->GetErrorMsg().c_str());
		if (strcmp (strerror(errno),"No such file or directory")== 0 )
		{
			throw TransactionException("", E_REPO_NOT_AVAILABLE) ;
		}
		else
		{
			throw TransactionException(pit->GetErrorMsg().c_str(), E_SYSTEMBUSY) ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ConfSchemaReaderWriter::Sync()
{
	std::set<std::string>::const_iterator readWriteGrpIter ;
	std::set<std::string> modifiedGrps = ConfSchemaMgr::GetInstance()->GetModifiedGroups() ;
	readWriteGrpIter = modifiedGrps.begin() ;

	while( readWriteGrpIter != modifiedGrps.end() )
	{
		std::string filename ;
		filename = schemaMgrPtr->GetSchemaFilePath(*readWriteGrpIter) ;
		m_filenames.insert( filename ) ;
		readWriteGrpIter++ ;
	}
	std::string transactionLogPath = schemaMgrPtr->getConfigDirLocation() + ACE_DIRECTORY_SEPARATOR_STR_A + "transaction.log" ;
	boost::shared_ptr<InmTransaction> pit ;
	pit.reset(new InmLogBasedTransaction(transactionLogPath, m_bkpmgr.get())) ;

	BeginTransaction(pit) ;
	schemaMgrPtr->WriteConfSchema() ;
	EndTransaction(pit) ;
}

TransactionException::TransactionException(const std::string& str, INM_ERROR errCode)
{
	m_whatStr = str ;
	m_errCode = errCode ;
}
const char* TransactionException::what() const
{
	return m_whatStr.c_str() ;
}
INM_ERROR TransactionException::getError() const
{
	return m_errCode ;
}

bool ConfSchemaReaderWriter::ActOnCorruptedGroups()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::set <std::string> corruptedGroups ;
	bool recoverableCorruption = true ;
	if ( m_ConfSchemaReaderWriterPtr->schemaMgrPtr->GetCorruptedGroups(corruptedGroups) )
	{
		std::string schemafilepath ; 
		std::string corruptedfilepath ;
		if( corruptedGroups.find( ALERTS_GROUP ) != corruptedGroups.end() )
		{
			schemafilepath = m_ConfSchemaReaderWriterPtr->schemaMgrPtr->GetSchemaFilePath(ALERTS_GROUP) ;
			corruptedfilepath = schemafilepath + ".corrupt" ;
			ACE_OS::unlink( corruptedfilepath.c_str() ) ;
		}
		AlertConfigPtr alertConfigPtr ;
		try
		{
			alertConfigPtr = AlertConfig::GetInstance() ;
		}
		catch(TransactionException& ex)
		{
			alertConfigPtr = AlertConfig::GetInstance() ;
		}
		std::set <std::string>::const_iterator grpIter ;
		grpIter = corruptedGroups.begin();
		while( grpIter != corruptedGroups.end() )
		{
			schemafilepath = m_ConfSchemaReaderWriterPtr->schemaMgrPtr->GetSchemaFilePath(*grpIter) ;
			std::string relpath = schemafilepath.substr( schemafilepath.find_last_of( '\\' ) + 1 ) ;
			std::string alertmsg ;
			alertmsg = m_ConfSchemaReaderWriterPtr->schemaMgrPtr->GetConfigNameInHumanFormat( *grpIter ) ;
			alertmsg += " configuration data is in inconsistent state";
			if( *grpIter == AUDITCONFIG_GROUP || 
				*grpIter == ALERTS_GROUP || 
				*grpIter == EVENTQUEUE_GROUP)

			{
				corruptedfilepath = schemafilepath + ".corrupt" ;	
				if (*grpIter != ALERTS_GROUP ) 
				{
					ACE_OS::unlink( corruptedfilepath.c_str() ) ;
				}
				alertmsg += " and it will be rebuild automatically." ;
				alertConfigPtr->AddAlert("NOTICE", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertmsg) ;
			}
			else
			{
				alertmsg += " and it must be restored immediately. Refer to the user guide for steps." ;
				alertConfigPtr->AddAlert("CRITICAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertmsg) ;
				recoverableCorruption = false ;
			}
			grpIter++ ;
		}
		m_ConfSchemaReaderWriterPtr->Sync() ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return recoverableCorruption ;
}