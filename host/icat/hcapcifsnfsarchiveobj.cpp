#include "archiveobject.h"
#include "icatexception.h"
#include "configvalueobj.h"
#include <time.h>
#include "archivecontroller.h"
#include "common.h"
#include "portablehelpers.h"
#ifdef SV_WINDOWS
#include<atlconv.h>
#endif

std::string CifsNfsArchiveObj::prepareDateStr()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	if( m_startTime.compare("") == 0 )
	{
		m_startTime = archiveController::getInstance()->getStartTime("%Y_%m_%d_%H_%M_%S") ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return m_startTime ;
}


std::string CifsNfsArchiveObj::prepareBasePath()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string baseUrl ;
	ConfigValueObject *configObj = ConfigValueObject::getInstance() ;
	std::string targetDir ;
	int autoGenDirName = configObj->getAutoGenDestDirectory() ;
	char pathSeperator ;
	
	if( ConfigValueObject::getInstance()->getTransport().compare("cifs") == 0 )
		pathSeperator = '\\' ;
	else
		pathSeperator = '/';

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
			targetDir = configObj->getBranchName()  + pathSeperator + configObj->getServerName() + pathSeperator + SrcVolume;
			targetDir += pathSeperator ;
			targetDir += m_startTime ;
		}
		else
		{
			targetDir = configObj->getTargetDirectory() ;
			if( targetDir.compare("") != 0 )
			{
				targetDir.erase( targetDir.find_last_not_of("/") + 1) ; //erase all trailing spaces
				targetDir.erase(0 , targetDir.find_first_not_of("/") ) ; //erase all leading spaces
				targetDir.erase( targetDir.find_last_not_of("\\") + 1) ; //erase all trailing spaces
				targetDir.erase(0 , targetDir.find_first_not_of("\\") ) ; //erase all leading spaces
			}
		}
		if( targetDir.compare("") != 0 )
		{
			baseUrl += targetDir ;
			baseUrl += pathSeperator ;
		}
		m_basePath  = baseUrl ;
	}
	else
	{
		boost::shared_ptr<ResumeTracker> tracker = archiveController::getInstance()->resumeTrackerInstance() ;
		//m_basePath = pathSeperator ;
		m_basePath += tracker->getNormalModeBasePath() ; 
		m_basePath += pathSeperator ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return m_basePath ;
}


void CifsNfsArchiveObj::prepareFullPath(const std::string& relPath, std::string& fullPath) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;  
    char pathSeperator ;
	if( ConfigValueObject::getInstance()->getTransport().compare("cifs") == 0 )
		pathSeperator = '\\' ;
	else
		pathSeperator = '/';

	if( m_basePath.compare("") == 0 )
	{
		prepareBasePath() ;
	}
	if( ConfigValueObject::getInstance()->getTransport().compare("cifs") == 0 )
	{
		fullPath = pathSeperator ;
		fullPath += pathSeperator ;
		fullPath += getArchiveAddress() ;
		fullPath += pathSeperator ;
		fullPath += archvieRootDirectory() ;
		fullPath += pathSeperator ;
	}
	else
	{
		fullPath += archvieRootDirectory() ;
		fullPath += pathSeperator ;
	}

	fullPath += m_basePath ;

	fullPath += relPath;
	if( ConfigValueObject::getInstance()->getTransport().compare("cifs") == 0 )
	{
		for (unsigned i=0; i<fullPath.length(); ++i) if (fullPath[i]=='/') fullPath[i]='\\';
	}
	else
	{
		for (unsigned i=0; i<fullPath.length(); ++i) if (fullPath[i]=='\\') fullPath[i]='/';
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

ICAT_OPER_STATUS CifsNfsArchiveObj::archiveFile(const fileSystemObj& obj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    fileSystemObj fileSysObj = const_cast <fileSystemObj&>(obj) ;
	double bytes_sent = 0;
    std::string srcFileNamePath = fileSysObj.absPath() ;
	std::string tgtFileNamePath ;
	ACE_HANDLE hd_in, hd_out ;
	ICAT_OPER_STATUS status = ICAT_OPSTAT_SUCCESS ;
	
	hd_in = hd_out = ACE_INVALID_HANDLE ;

	std::string pathSeperator ;
	if( ConfigValueObject::getInstance()->getTransport().compare("cifs") == 0 )
		pathSeperator = "\\" ;
	else
		pathSeperator = "/";

    prepareFullPath(fileSysObj.getRelPath(), tgtFileNamePath) ;
    std::string dirName = tgtFileNamePath.substr(0, tgtFileNamePath.find_last_of(pathSeperator)) ;
   
    SVERROR rc = SVMakeSureDirectoryPathExists(dirName.c_str()) ;
    if( rc.failed() )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to Create Directory: %s. Error Returned: %s\n", dirName.c_str(), ACE_OS::strerror(rc.error.ntstatus)) ;						
        return ICAT_OPSTAT_FAILED ;
    }
#ifdef SV_WINDOWS
    USES_CONVERSION ;
    std::string tempTarget = "\\\\?\\UNC" ;
    tempTarget += tgtFileNamePath.c_str() + 1 ;
    tgtFileNamePath = tempTarget ;
    std::wstring srcFileNameW = A2W( srcFileNamePath.c_str() ) ;
    std::wstring tgtFileNameW = A2W( tgtFileNamePath.c_str() ) ;

    hd_in = ACE_OS::open( srcFileNameW.c_str(), O_RDONLY ) ;
    hd_out = ACE_OS::open( tgtFileNameW.c_str(), O_CREAT | O_WRONLY) ;
#else
    hd_in = ACE_OS::open(getLongPathName(srcFileNamePath.c_str()).c_str(), O_RDONLY) ;
    hd_out = ACE_OS::open(getLongPathName(tgtFileNamePath.c_str()).c_str(), O_CREAT | O_WRONLY) ;
#endif

	if( hd_in != ACE_INVALID_HANDLE )
	{
		if( hd_out != ACE_INVALID_HANDLE )
		{
			size_t bytesRead = 0 ;
            char *buf = new char[1024*1024] ;
			while( ( bytesRead = ACE_OS::read(hd_in, buf, 1024*1024) ) != 0 )
			{
				size_t bytesWrote = 0 ;
				if( ( bytesWrote = ACE_OS::write(hd_out, buf, bytesRead)) != bytesRead )
				{
					DebugPrintf(SV_LOG_ERROR, "Among %d bytes wrote only %d bytes\n", bytesRead, bytesWrote) ;
					status = ICAT_OPSTAT_FAILED ;
				}
				bytes_sent += bytesWrote;
			}
            delete[] buf ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to Open %s for writing. Error %s\n", tgtFileNamePath.c_str(), ACE_OS::strerror(ACE_OS::last_error())) ;			
			status = ICAT_OPSTAT_FAILED ;
		}
	}
	else
	{
        DebugPrintf(SV_LOG_ERROR, "Failed to Open %s for writing. Error %s\n", srcFileNamePath.c_str(), ACE_OS::strerror(ACE_OS::last_error())) ;			
        status = ICAT_OPSTAT_FAILED ;
	}

	SAFE_ACE_CLOSE_HANDLE(hd_in) ;
	SAFE_ACE_CLOSE_HANDLE(hd_out) ;

	if( status != ICAT_OPSTAT_SUCCESS )
	{
		// PR#10815: Long Path support
		ACE_OS::unlink( getLongPathName(tgtFileNamePath.c_str()).c_str() ) ; //if there are any errors we should unlink any partially created file
    }
    else
    {
        archiveControllerPtr arc_controller = archiveController::getInstance() ;
        arc_controller->m_tranferStat.total_Num_Of_Bytes_Sent += bytes_sent;
        arc_controller->m_tranferStat.total_Num_Of_Transfered_Files++;
        DebugPrintf(SV_LOG_DEBUG, " ----Statistics for %s file---- \n",fileSysObj.absPath().c_str()) ;
        DebugPrintf(SV_LOG_DEBUG, " Transferred Bytes: %lf bytes \n", bytes_sent) ;

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return status ;
}

ICAT_OPER_STATUS CifsNfsArchiveObj::archiveDirectory(const fileSystemObj& obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	return ICAT_OPSTAT_FAILED ;
}

ICAT_OPER_STATUS CifsNfsArchiveObj::deleteFile(const fileSystemObj& obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ICAT_OPER_STATUS status = ICAT_OPSTAT_FAILED ; 
	std::string tgtPath ;
	fileSystemObj fileSysObj = const_cast <fileSystemObj&>(obj) ;
	char pathSeperator ;
	if( ConfigValueObject::getInstance()->getTransport().compare("cifs") == 0 )
		pathSeperator = '\\' ;
	else
		pathSeperator = '/';


	if( fileSysObj.getOperation() == ICAT_OPER_ARCH_FILE )
	{
		prepareFullPath(fileSysObj.getRelPath(), tgtPath) ;
	}
	else
	{
		if( ConfigValueObject::getInstance()->getTransport().compare("cifs") == 0 )
		{
			tgtPath = pathSeperator ;
			tgtPath += pathSeperator ;
			tgtPath += getArchiveAddress() ;
			tgtPath += pathSeperator ;
			tgtPath += archvieRootDirectory() ;
			tgtPath += pathSeperator ;
			tgtPath += fileSysObj.getUrl()  ;
			for (unsigned i=0; i < tgtPath.length(); ++i) if (tgtPath[i]=='/') tgtPath[i]='\\';
		}
		else
		{
			tgtPath = pathSeperator ;
			tgtPath += archvieRootDirectory() ;
			tgtPath += pathSeperator ;
			tgtPath += fileSysObj.getUrl()  ;
			for (unsigned i=0; i < tgtPath.length(); ++i) if (tgtPath[i]=='\\') tgtPath[i]='/';
		}
	}
	
	ACE_stat stat ;
	// PR#10815: Long Path support
	if( sv_stat(getLongPathName(tgtPath.c_str()).c_str(), &stat ) != 0 )
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to stat %s. %s\n", tgtPath.c_str(), ACE_OS::strerror(ACE_OS::last_error())) ;
	}
	else
	{
		if( stat.st_mode & S_IFDIR )
		{
			// PR#10815: Long Path support
			if( ACE_OS::rmdir(getLongPathName(tgtPath.c_str()).c_str()) == 0 )
			{
				DebugPrintf(SV_LOG_DEBUG, "Deleted. %s\n", tgtPath.c_str()) ;
                status = ICAT_OPSTAT_DELETED ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to delete %s. %s\n", tgtPath.c_str(), ACE_OS::strerror(ACE_OS::last_error())) ;
			}
		}
		else
		{
			// PR#10815: Long Path support
			if( ACE_OS::unlink(getLongPathName(tgtPath.c_str()).c_str()) == 0 )
			{
				DebugPrintf(SV_LOG_DEBUG, "Deleted. %s\n", tgtPath.c_str()) ;
                status = ICAT_OPSTAT_DELETED ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to delete %s. %s\n", tgtPath.c_str(), ACE_OS::strerror(ACE_OS::last_error())) ;
			}
		}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return status ;
}

ICAT_OPER_STATUS CifsNfsArchiveObj::deleteDirectory(const fileSystemObj& obj)
{
	return ICAT_OPSTAT_FAILED ;
}

ICAT_OPER_STATUS CifsNfsArchiveObj::specifyMetaData(const fileSystemObj& obj)
{
	return ICAT_OPSTAT_FAILED ;
}

