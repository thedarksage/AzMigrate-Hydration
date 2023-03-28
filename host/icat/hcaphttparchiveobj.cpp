#include "archiveobject.h"
#include "icatexception.h"
#include "configvalueobj.h"
#include <time.h>
#include "archivecontroller.h"
#include "common.h"

std::string HttpArchiveObj::m_baseUrl ;
std::string HttpArchiveObj::m_startTime ;
std::string CifsNfsArchiveObj::m_basePath ;
std::string CifsNfsArchiveObj::m_startTime ;


void archiveObj::setArchiveAddress(const std::string& address)
{
	m_archiveAddress = address ;
}
void archiveObj::setPort(int port)
{
	m_port = port ;
}
void archiveObj::setArchiveRootDirectory(const std::string& rootDirectory)
{
	m_archiveRootDir = rootDirectory ;
}
std::string archiveObj::getArchiveAddress()
{
	return m_archiveAddress ;
}
int archiveObj::getPort(int port)
{
	return m_port ;
}
std::string archiveObj::archvieRootDirectory()
{
	return m_archiveRootDir ;
}


std::string HttpArchiveObj ::prepareDateStr()
{
	if( m_startTime.compare("") == 0 )
	{
		m_startTime = archiveController::getInstance()->getStartTime("%Y_%m_%d_%H_%M_%S") ;
	}
	return m_startTime ;
}

ArchiveObj::ArchiveObj()
{
}
ArchiveObj::ArchiveObj(const std::string& archiveAddress)
{     
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_archiveAddress = archiveAddress ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

HttpArchiveObj::HttpArchiveObj()
{

}



bool HttpArchiveObj::init()
{
	return m_transport.initTransport() ;
}
ICAT_OPER_STATUS HttpArchiveObj::archiveFile(const fileSystemObj& obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ICAT_OPER_STATUS status = ICAT_OPSTAT_NONE ;
	std::string url ="" ;
	fileSystemObj fileSysObj = const_cast <fileSystemObj&>(obj) ;
	prepareDataUrl(fileSysObj.getRelPath(), url) ;
	try
	{
		m_transport.uploadFile(obj, url) ;
		status = ICAT_OPSTAT_SUCCESS ;
		DebugPrintf(SV_LOG_DEBUG, "Upload success.. %s\n", url.c_str()) ;
	}
	catch(icatHttpTransportException &ex)
	{
		if( ex.getCode() == CURLE_OK)
		{
			switch( ex.getResponseCode() )
			{
				case 200:
					break ;
				case 400:
					DebugPrintf(SV_LOG_ERROR, "Bad Request. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_BADREQUEST ;
					break ;
				case 403:
					DebugPrintf(SV_LOG_ERROR, "Forbidden. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_FORBIDDEN ;
					break ;
				case 409 :
					DebugPrintf(SV_LOG_WARNING, "File already exists at server. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_ALREADY_EXISTS ;
					break ;
				case 413:
					DebugPrintf(SV_LOG_ERROR, "File Too large. There might be no space left in server. File Size : "ULLSPEC"Url %s\n", fileSysObj.getStat().st_size, url.c_str()) ;
					status = ICAT_OPSTAT_FILE_TOOLARGE ;
					break ;
				case 414:
					DebugPrintf(SV_LOG_ERROR, "URI Too long. url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_URI_TOOLONG ;
					break ;
				case 500:
					DebugPrintf(SV_LOG_ERROR, "Server Error. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_SERVER_ERROR ;
					break ;
				case 503:
					DebugPrintf(SV_LOG_ERROR, "Service Unavailable %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_SERVICE_UNAVAILABLE ;
					break ;
				default:
					status = ICAT_OPSTAT_FAILED ;
			}
		}
		else
		{
			switch( ex.getCode() )
			{
			case CURLE_OPERATION_TIMEDOUT :
				DebugPrintf(SV_LOG_ERROR, "Connection to %s Time Out\n",getArchiveAddress().c_str()) ;
				status = ICAT_OPSTAT_CONN_TIMEOUT ;
				break ;
			case CURLE_COULDNT_CONNECT :
				DebugPrintf(SV_LOG_ERROR, "Could not connect to server %s\n", getArchiveAddress().c_str()) ;
				status = ICAT_OPSTAT_COULDNOT_CONNECT ;
				break ;
			default:
				status = ICAT_OPSTAT_FAILED ;
			}
		}
	}
	catch(icatTransportException &ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Bad Transport %s.. Re-initializing it\n", ex.what()) ;
		if( init() )
		{
			DebugPrintf(SV_LOG_DEBUG, "Bad transport and transport Re-Initialized\n") ;
			status = ICAT_OPSTAT_BAD_TRANSPORT ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to Re-Initialize Transport\n") ;
			status = ICAT_OPSTAT_FAILED ;
		}
	}
	catch(icatFileNotFoundException &ex)
	{
		status = ICAT_OPSTAT_NOTREADABLE ;
		DebugPrintf(SV_LOG_ERROR, "Problem with File I/O for %s. Error %s\n", fileSysObj.absPath().c_str(), ex.what()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return status ;
}

ICAT_OPER_STATUS HttpArchiveObj::archiveDirectory(const fileSystemObj& obj)
{
	return ICAT_OPSTAT_FAILED ;
}

ICAT_OPER_STATUS HttpArchiveObj::deleteFile(const fileSystemObj& obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ICAT_OPER_STATUS status = ICAT_OPSTAT_NONE ; 
	std::string url ="" ;
	fileSystemObj fileSysObj = const_cast <fileSystemObj&>(obj) ;
	if( fileSysObj.getOperation() == ICAT_OPER_ARCH_FILE )
	{
		prepareDataUrl(fileSysObj.getRelPath(), url) ;
	}
	else
	{
		url = "http://" ;
		url += getArchiveAddress() ;
		url += "/" ;
		url += archvieRootDirectory() ;
		url += "/" ;
		url += fileSysObj.getUrl()  ;
		for (unsigned i=0; i<url.length(); ++i) if (url[i]=='\\') url[i]='/';
	}
	
	try
	{
		m_transport.deleteFile(url) ;
		DebugPrintf(SV_LOG_DEBUG, "Delete success.. %s\n", url.c_str()) ;
		status = ICAT_OPSTAT_DELETED ;
	}
	catch(icatHttpTransportException &ex)
	{
		if( ex.getCode() == CURLE_OK )
		{
			switch( ex.getResponseCode() )
			{
				case 200:
					break ;
				case 400:
					DebugPrintf(SV_LOG_ERROR, "Bad Request. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_BADREQUEST ;
					break ;
				case 403:
					DebugPrintf(SV_LOG_ERROR, "Forbidden. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_FORBIDDEN ;
					break ;
				case 404:
					DebugPrintf(SV_LOG_WARNING, "File not found. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_DELETED ;
					break ;
				case 409 :
					DebugPrintf(SV_LOG_WARNING, "File already exists. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_ALREADY_EXISTS ;
					break ;
				case 413:
					DebugPrintf(SV_LOG_ERROR, "File Too large. There might be no space left in server. File Size : "ULLSPEC"Url %s\n", fileSysObj.getStat().st_size, url.c_str()) ;
					status = ICAT_OPSTAT_FILE_TOOLARGE ;
					break ;
				case 414:
					DebugPrintf(SV_LOG_ERROR, "URI Too long. %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_URI_TOOLONG ;
					break ;
				case 500:
					DebugPrintf(SV_LOG_ERROR, "Server Error. %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_SERVER_ERROR ;
					break ;
				case 503:
					DebugPrintf(SV_LOG_ERROR, "Service Unavailable. Url %s\n", url.c_str()) ;
					status = ICAT_OPSTAT_SERVICE_UNAVAILABLE ;
					break ;
				default:
					status = ICAT_OPSTAT_FAILED ;
			}
		}
		else
		{
			switch( ex.getCode() )
			{
				case CURLE_OPERATION_TIMEDOUT :
					DebugPrintf(SV_LOG_ERROR, "Connection to %sTimed out\n", getArchiveAddress().c_str()) ;
					status = ICAT_OPSTAT_CONN_TIMEOUT ;
					break ;
				case CURLE_COULDNT_CONNECT :
					DebugPrintf(SV_LOG_ERROR, "Couldnot connect to server %s\n", getArchiveAddress().c_str()) ;
					status = ICAT_OPSTAT_COULDNOT_CONNECT ;
					break ;
				default:
					status = ICAT_OPSTAT_FAILED ;
			}
		}
	}
	catch(icatTransportException &ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Bad Transport %s.. Re-initializing it\n", ex.what()) ;
		if( init() )
		{
			DebugPrintf(SV_LOG_DEBUG, "Bad transport and transport Re-Initialized\n") ;
			status = ICAT_OPSTAT_BAD_TRANSPORT ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to Re-Initialize Transport\n") ;
			status = ICAT_OPSTAT_FAILED ;
		}
	}
	catch(icatFileNotFoundException &ex)
	{
		status = ICAT_OPSTAT_NOTREADABLE ;
		DebugPrintf(SV_LOG_ERROR, "Problem with File I/O for %s. Error %s\n", fileSysObj.absPath().c_str(), ex.what()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return status ;
}

ICAT_OPER_STATUS HttpArchiveObj::deleteDirectory(const fileSystemObj& obj)
{
	return ICAT_OPSTAT_FAILED ;
}
ICAT_OPER_STATUS HttpArchiveObj::specifyMetaData(const fileSystemObj& obj)
{
	return ICAT_OPSTAT_FAILED ; ;
}

std::string HttpArchiveObj::prepareBaseUrl()
{
	std::string baseUrl ;
	ConfigValueObject *configObj = ConfigValueObject::getInstance() ;
	std::string targetDir ;
	int autoGenDirName = configObj->getAutoGenDestDirectory() ;
	if( !configObj->getResume() )
	{
		if( m_startTime.compare("") == 0 )
		{
			prepareDateStr() ;
		}
		if( autoGenDirName )
		{
			std::string SrcVolume =  configObj->getSourceVolume() ;
			formatingSrcVolume(SrcVolume);
			targetDir = configObj->getBranchName()  + "/" + configObj->getServerName() + "/" + SrcVolume ;
			targetDir += "/" ;
			targetDir += m_startTime ;
		}
		else
		{
			targetDir = configObj->getTargetDirectory() ;
			if( targetDir.compare("") != 0 )
			{
				targetDir.erase( targetDir.find_last_not_of("/") + 1) ; //erase all trailing spaces
				targetDir.erase(0 , targetDir.find_first_not_of("/") ) ; //erase all leading spaces
			}
		}
		if( targetDir.compare("") != 0 )
		{
			baseUrl += targetDir ;
			baseUrl += "/" ;
		}
		m_baseUrl  = baseUrl ;
	}
	else
	{
		boost::shared_ptr<ResumeTracker> tracker = archiveController::getInstance()->resumeTrackerInstance() ;
		//m_baseUrl = "/" ;
		m_baseUrl += tracker->getNormalModeBasePath() ; 
		m_baseUrl += "/" ;
	}
	return m_baseUrl ;
}

void HttpArchiveObj::prepareDataUrl(const std::string& relPath, std::string& url) 
{
	if( m_baseUrl.compare("") == 0 )
	{
		prepareBaseUrl() ;
	}
	url = "http://" ;
	url += getArchiveAddress() ;
	url += "/" ;
	url += archvieRootDirectory() ;
	url += "/" ;
	url += m_baseUrl ;

	url += relPath ;
	for (unsigned i=0; i<url.length(); ++i) if (url[i]=='\\') url[i]='/';
}

