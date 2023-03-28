#include "APIController.h"
#include "InmXmlParser.h"
#include <ace/Guard_T.h>
#include <ace/File_Lock.h>
#include "confengine/confschemareaderwriter.h"
#include "inmageex.h"
#include "inmsdkutil.h"
#include "service.h"
#include "portablehelpersmajor.h"
ACE_Atomic_Op<ACE_Thread_Mutex, bool> RepositoryLock::m_quit = false ;
RepositoryLock::RepositoryLock(const std::string& lockfile)
{
	m_lockpath = lockfile ;
#ifdef SV_WINDOWS
	m_lockhandle = INVALID_HANDLE_VALUE ;
#endif
	m_locked = false ;
}

bool RepositoryLock::Acquire(bool processanyways)
{
	DebugPrintf(SV_LOG_DEBUG, "Acquiring file lock at %S\n", getLongPathName(m_lockpath.c_str()).c_str()) ;
	bool bProcessEvenWithoutlock = false ;
	int count = 0 ;
	INM_ERROR eCode = E_SUCCESS ;
	do
	{
#ifdef SV_WINDOWS
		m_lockhandle = CreateFileW(getLongPathName(m_lockpath.c_str()).c_str(),
								GENERIC_READ|GENERIC_WRITE,
								FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
								CREATE_ALWAYS, 
								0, 
								NULL);
		if(m_lockhandle != INVALID_HANDLE_VALUE)
		{
			memset(&o, 0, sizeof(o));
			DebugPrintf(SV_LOG_DEBUG, "Before Calling LockFileEx\n") ;
			if(LockFileEx(m_lockhandle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &o))
			{
				m_locked = true ;
				eCode = E_SUCCESS ;
				DebugPrintf(SV_LOG_DEBUG, "acquired lock\n") ;
				break ;
			}
			else if( ERROR_LOCK_VIOLATION == GetLastError() )
			{
				DebugPrintf(SV_LOG_ERROR, "File lock %S held by another thread or process.. waiting for it to be released\n", getLongPathName(m_lockpath.c_str()).c_str(), GetLastError()) ;
				ACE_OS::sleep(1) ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Unable to hold the file lock %S, %ld\n", getLongPathName(m_lockpath.c_str()).c_str(), GetLastError()) ;
				ACE_OS::sleep(2) ;
				eCode = E_REPO_UNABLETOLOCK ;
				count++ ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to create the file %S, %ld\n",getLongPathName( m_lockpath.c_str()).c_str(), GetLastError()) ;
			ACE_OS::sleep(1) ;
			eCode = E_REPO_NORW_ACCESS ;
			count++ ;
		}
#else
		m_fd = open(m_lockpath.c_str(),O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if(m_fd > 0)
		{
			struct flock lock;
			lock.l_start = 0;
			lock.l_len = 0; // len = 0 means entire file
			lock.l_type = F_WRLCK;
			lock.l_whence = SEEK_SET;
			if (fcntl(m_fd, F_SETLKW, &lock) == -1)
			{
				close(m_fd);
			}
			else
			{
				m_locked = true ;
				DebugPrintf(SV_LOG_DEBUG, "acquired lock\n") ;
				break ;
			}
		}

#endif
		if( count > 3 )
		{
			if( processanyways )
			{
				DebugPrintf(SV_LOG_DEBUG, "Continuing ahead even without lock\n") ;
				bProcessEvenWithoutlock = true ;
				break ;
			}
			throw TransactionException("Unable to synchronize the access to backup location", eCode) ;
		}
#ifdef SV_WINDOWS
		if( m_lockhandle != INVALID_HANDLE_VALUE )
		{
			CloseHandle( m_lockhandle ) ;
			m_lockhandle = INVALID_HANDLE_VALUE  ;
		}
#else
        if (-1 != m_fd) {
		    close(m_fd);
            m_fd = -1;
        }
#endif
		if( m_quit.value() )
		{
			DebugPrintf(SV_LOG_WARNING, "Skipping the repository lock as the application is exiting\n") ;
			throw TransactionException("Skipping the repository lock as the application is exiting", E_INTERNAL) ;
		}
		
	} while( true ) ;
	return bProcessEvenWithoutlock ;
}

void RepositoryLock::Release()
{
	if( m_locked )
	{
#ifdef SV_WINDOWS
		int retries = 0 ;
		do
		{
			retries++ ;
			if( UnlockFileEx(m_lockhandle, 0, 1, 0, &o) )
			{
				DebugPrintf(SV_LOG_DEBUG, "Releasing file lock %s\n", m_lockpath.c_str()) ;
				break ;
			}
			else if( GetLastError() == ERROR_NOT_LOCKED )
			{
				DebugPrintf(SV_LOG_ERROR, "This is not a error... Treating error 158 as file lock not held and no need to retry the unlock again\n") ;
				break ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Unlock %S failed %ld\n", getLongPathName(m_lockpath.c_str()).c_str(), GetLastError()) ;
				ACE_OS::sleep(2) ;
			}
			if( retries > 3 )
			{
				DebugPrintf(SV_LOG_ERROR, "repository un-lock failed even after %d attempts.. Restart the application..\n", retries) ;
				ACE_OS::sleep(1) ;
				break ;
			}
			if( m_quit.value() )
			{
				DebugPrintf(SV_LOG_WARNING, "Skipping the repository un-lock as the application is exiting\n") ;
				//throw TransactionException("Skipping the repository un-lock as the application is exiting", E_INTERNAL) ;
				break ;
			}
		} while( true ) ;
		if( m_lockhandle != INVALID_HANDLE_VALUE )
		{
			CloseHandle(m_lockhandle);
			m_lockhandle = INVALID_HANDLE_VALUE ;
		}
		m_locked = false ;
#else
		struct flock lock;
		lock.l_start = 0;
		lock.l_len = 0; // len = 0 means entire file
		lock.l_type = F_UNLCK;
		lock.l_whence = SEEK_SET;
		fcntl(m_fd, F_SETLK, &lock) ;
		close(m_fd);
	m_locked = false ;
#endif
	}
}

RepositoryLock::~RepositoryLock()
{
	Release() ;
}

APIController::APIController(void)
{
}
APIController::~APIController(void)
{
}
bool getRepositoryPath(FunctionInfo& fundata, std::string& repositoryPath)
{
	bool repoSet = false ;
	if(fundata.m_RequestPgs.m_Params.find("RepositoryPath") != fundata.m_RequestPgs.m_Params.end() )
	{
		repositoryPath = fundata.m_RequestPgs.m_Params.find("RepositoryPath")->second.value ;
		repoSet = true ;
	}
	else if( fundata.m_RequestPgs.m_Params.find("RepositoryDeviceName") != fundata.m_RequestPgs.m_Params.end() )
	{
		repositoryPath = fundata.m_RequestPgs.m_Params.find("RepositoryDeviceName")->second.value ;
		repoSet = true ;
	}
	else
	{
		try
		{
			LocalConfigurator lc ;
			repositoryPath = lc.getRepositoryLocation() ;
			DebugPrintf(SV_LOG_DEBUG, "Reading the repository path from drscout.conf apiname %s\n", fundata.m_RequestFunctionName.c_str()) ;
			repoSet = true ;
		}
		catch(ContextualException& ex)
		{
			DebugPrintf(SV_LOG_DEBUG, "%s\n", ex.what()) ;
		}
	}
	return repoSet; 
}

bool isRepositoryRequired(FunctionInfo &FunctionData)
{
	bool repoRequired = true ;
	std::string& funname = FunctionData.m_RequestFunctionName ;
	if( InmStrCmp<NoCaseCharCmp>(funname .c_str(), "ListProtectableVolumes") == 0 ||
		InmStrCmp<NoCaseCharCmp>(funname .c_str(), "ListRepositoryDevices") == 0 ||
		InmStrCmp<NoCaseCharCmp>(funname .c_str(), "SetupRepository") == 0 ||
		InmStrCmp<NoCaseCharCmp>(funname .c_str(), "SpaceRequired") == 0 ||
		InmStrCmp<NoCaseCharCmp>(funname .c_str(), "UpdateCredentials") == 0 )
	{
		repoRequired = false ;
	}
	DebugPrintf(SV_LOG_DEBUG, "api name %s, repository required %d\n", funname.c_str(), repoRequired) ;

	return repoRequired ;
}

bool isRepositoryAccessible(INM_ERROR& err, std::string& errMsg, std::string& repoPath, std::string& hostid)
{
	std::string configStorePath = constructConfigStoreLocation( getRepositoryPath(repoPath),  hostid )  ;
	if( !isShareReadWriteable(repoPath, err, errMsg) )
	{
		AccessRemoteShare(getRepositoryPath(repoPath), err, errMsg ) ;
	}
	if( err == E_SUCCESS && SVMakeSureDirectoryPathExists(getRepositoryPath(repoPath).c_str()) != SVS_OK )
	{
		err = E_REPO_NOT_AVAILABLE ;
		errMsg = "Unable to find availability of " ; 
		errMsg += configStorePath ;
		errMsg += ". Backup location not available. " ;
	}
	if( err == E_SUCCESS )
	{
		return true ;
	}
	else
	{
		return false ;
	}
}

std::string getHostId(FunctionInfo &FunctionData) 
{
	if( FunctionData.m_RequestPgs.m_Params.find("AgentGUID") != FunctionData.m_RequestPgs.m_Params.end() )
	{
		return FunctionData.m_RequestPgs.m_Params.find("AgentGUID")->second.value ;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Reading the host id from drscout.conf. apiname %s\n", FunctionData.m_RequestFunctionName.c_str()) ;
		LocalConfigurator lc ;
		return lc.getHostId() ;
	}
}

bool processAnyways(FunctionInfo &FunctionData)
{
	std::string& funname = FunctionData.m_RequestFunctionName ;
	bool bret = false ;
	if( InmStrCmp<NoCaseCharCmp>(funname.c_str(), "DownloadAlerts") == 0  ||
		InmStrCmp<NoCaseCharCmp>(funname.c_str(), "ListConfiguredRepositories") == 0  ||
		InmStrCmp<NoCaseCharCmp>(funname.c_str(), "UpdateCredentials") == 0 || 
		InmStrCmp<NoCaseCharCmp>(funname.c_str(), "ChangeBackupLocation") == 0 ||		
		InmStrCmp<NoCaseCharCmp>(funname.c_str(), "DeleteRepository") == 0 )
	{
		bret = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "api name %s, processAnyways %d\n", funname.c_str(), bret) ;
	return bret ;
}
bool shouldStopService(FunctionInfo &FunctionData, std::string& servicestopreason)
{
	bool bret = false ;
	std::string& funname = FunctionData.m_RequestFunctionName ;
	/*if( InmStrCmp<NoCaseCharCmp>(funname.c_str(), "MoveRepository") == 0  )
	{
		servicestopreason = "Moving the backup location contents" ;
		bret = true ;
	}
	else */if( InmStrCmp<NoCaseCharCmp>(funname.c_str(), "ChangeBackupLocation") == 0  )
	{
		servicestopreason = "Changing the backup location" ;
		bret = true ;
	}
	else if( InmStrCmp<NoCaseCharCmp>(funname.c_str(), "DeleteRepository") == 0 )
	{
		servicestopreason = "Deleting the backup location contents" ;
		bret = true ;
	}
	else if( InmStrCmp<NoCaseCharCmp>(funname.c_str(), "UpdateCredentials") == 0 )
	{
		servicestopreason = "Updating the backup location access credentials" ;
		bret = true ;
	}
	/*
	else if( InmStrCmp<NoCaseCharCmp>(funname.c_str(), "ArchiveRepository") == 0 )
	{
		servicestopreason = "Archiving the backup location contents" ;
		bret = true ;
	}*/

	DebugPrintf(SV_LOG_DEBUG, "api name %s, should stop service %d\n", funname.c_str(), bret) ;
	return bret ;
}
void SignalApplicationQuit(bool flag)
{
	RepositoryLock::RequestQuit(flag) ;
}
void ProcessAPI(FunctionInfo &FunctionData)
{
	boost::shared_ptr<Handler> pHandler ;
	INMAGE_ERROR_CODE funcErrCode = E_SUCCESS;
	pHandler = HandlerFactory::getHandler(FunctionData.m_RequestFunctionName,
		FunctionData.m_FuntionId,
		funcErrCode);
	bool bShouldStopSvc ;
	if(E_SUCCESS == funcErrCode)
	{
		std::string repopath, hostId, errMsg,servicestopreason ;
		bool isRepoConfigured = false ;
		INM_ERROR repoAccessError = E_SUCCESS ;
		bool bIsRepoConfigured = getRepositoryPath(FunctionData, repopath) ;
		DebugPrintf(SV_LOG_DEBUG, "processAPI repository path %s\n", repopath.c_str()) ;
		hostId = getHostId(FunctionData) ;
		DebugPrintf(SV_LOG_DEBUG, "processAPI host id %s\n", hostId.c_str()) ;
		bool bIsRepoRequired = isRepositoryRequired(FunctionData) ;
		bShouldStopSvc = shouldStopService(FunctionData, servicestopreason) ;
		bool bProcessAnyways = processAnyways(FunctionData) ;
		do
		{
			if( bShouldStopSvc )
			{
#ifdef SV_WINDOWS
				StopSvc("backupagent") ;
				InmServiceStatus status = INM_SVCSTAT_NONE;
				getServiceStatus( "backupagent", status) ;
				if( INM_SVCSTAT_STOPPED != status && 
					status != INM_SVCSTAT_NONE && 
					status != INM_SVCSTAT_NOTINSTALLED)
				{
					funcErrCode =  E_SERVICE_RUNNING ;
					FunctionData.m_ReturnCode = getStrErrCode(E_SERVICE_RUNNING);
					FunctionData.m_Message = "Unable to stop backupagent service. Please stop it manually and retry again" ;
					break ;
				}
				UpdateServiceStopReason(servicestopreason) ;
#endif
			}

			if( bIsRepoRequired && !bIsRepoConfigured )
			{
				DebugPrintf(SV_LOG_DEBUG, "Repository not configured\n") ;
				funcErrCode =  E_NO_REPO_CONFIGERED ;
				break ;
			}
			bool bIsRepoAccessible = false ;
			if( bIsRepoConfigured )
			{
				bIsRepoAccessible = isRepositoryAccessible(repoAccessError, errMsg, repopath, hostId) ;
			}
			if( !bIsRepoAccessible )
			{
				DebugPrintf(SV_LOG_DEBUG, "backup location %s is not accessible\n", repopath.c_str()) ;
				funcErrCode =  repoAccessError ;
				break ;
			}
		} while( false ) ;
		std::string repoLockPath, repoLockPathForHost, repoLocationForHost  ;
		if( funcErrCode == E_SUCCESS || ( bProcessAnyways && repoAccessError != E_SUCCESS ) )
		{
			try
			{
				bool bRequireRepoLock = false ;
				bool bRequireRepoLockForHost = false ;
				if( !repopath.empty() && !hostId.empty())
				{
					repoLocationForHost = constructConfigStoreLocation( getRepositoryPath(repopath),  hostId ) ;
				}
				if( !repoLocationForHost.empty() )
				{
					WriteRequestToLog(FunctionData, repoLocationForHost) ;
				}
				bRequireRepoLock = requireRepoLock( FunctionData, getRepositoryPath(repopath),  repoLockPath) ;
				if( !repoLocationForHost.empty() )
				{
					bRequireRepoLockForHost = requireRepoLockForHost(FunctionData, repoLocationForHost, repoLockPathForHost ) ;
				}
				RepositoryLock repolock( repoLockPath ) ;
				RepositoryLock repohostlock( repoLockPathForHost ) ;
				bool bProcessEvenWithoutlock = false ;
				if( bRequireRepoLock )
				{
					repolock.Acquire(bProcessAnyways) ;
				}
				if( bRequireRepoLockForHost )
				{
					bProcessEvenWithoutlock = repohostlock.Acquire(bProcessAnyways) ;
				}
				if( bProcessEvenWithoutlock )
				{
					ValueType vt ;
					vt.value = "Yes" ;
					FunctionData.m_RequestPgs.m_Params.insert(std::make_pair("ProcessWithoutlock",  vt)) ;
				}
				ConfSchemaReaderWriter::CreateInstance(FunctionData.m_RequestFunctionName, getRepositoryPath(repopath), repoLocationForHost, bProcessEvenWithoutlock) ;
				if( shouldReleaseHostLockAfterRollingBackTransaction(FunctionData.m_RequestFunctionName) )
				{
					repohostlock.Release() ;
				}
				funcErrCode = pHandler->process(FunctionData);
			}
			catch(const char* ex)
			{
				DebugPrintf(SV_LOG_ERROR, "processRequestWithStream failed %s\n", ex) ;
				funcErrCode = E_INTERNAL ;
			}
			catch(const ContextualException& ex)
			{
				DebugPrintf(SV_LOG_ERROR, "processRequestWithStream failed %s\n", ex.what()) ;
				funcErrCode = E_INTERNAL ;
			}
			catch(const std::exception& ex)
			{
				DebugPrintf(SV_LOG_ERROR, "processRequestWithStream failed %s\n", ex.what()) ;
				funcErrCode = E_INTERNAL ;
			}
			catch(const TransactionException& ex)
			{
				if (FunctionData.m_RequestFunctionName.compare( "MoveRepository" ) == 0)
				{
					if (strcmp ( ex.what(),"Configuration store corrupted") == 0 ) 
					{
						std::string msg = "Cannot move the backup location contents as configuration data is Inconsistent.\n"; 
						msg += "Use bxadmin.exe --rebuildconfigstore to rebuild it and try again...\n";
						msg += "Exiting..";
						DebugPrintf(SV_LOG_ERROR, "%s\n", msg.c_str()) ;
						StartSvc("backupagent") ;
					}
				}
				else 
				{
					DebugPrintf(SV_LOG_ERROR, "processRequestWithStream failed %s\n", ex.what()) ;
				}
				funcErrCode = ex.getError() ;
			}
		}
		funcErrCode = repoAccessError == E_SUCCESS ?  funcErrCode : repoAccessError ;
		FunctionData.m_ReturnCode = getStrErrCode(funcErrCode) ;
		FunctionData.m_Message = getErrorMessage(funcErrCode) + " " + FunctionData.m_Message ;
		if( !repoLocationForHost.empty() )
		{
			WriteResponseToLog(FunctionData, repoLocationForHost) ;
		}
	}
	else
	{
		FunctionData.m_ReturnCode = getStrErrCode(funcErrCode) ;
		FunctionData.m_Message = getErrorMessage(funcErrCode) ;
	}
	if( bShouldStopSvc )
	{
#ifdef SV_WINDOWS
		std::string reason ;
		UpdateServiceStopReason(reason) ;
#endif
	}
}

INM_ERROR APIController::processRequest(FunctionInfo &FunctionData)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DebugPrintf(SV_LOG_DEBUG, "Function Name is %s\n",FunctionData.m_RequestFunctionName.c_str()) ;
	ProcessAPI(FunctionData) ;
	INMAGE_ERROR_CODE funcErrCode = static_cast<INMAGE_ERROR_CODE>(boost::lexical_cast<int>(FunctionData.m_ReturnCode)) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return funcErrCode;
}

INM_ERROR APIController::processRequest(std::list<FunctionInfo> &listFuncReq)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INMAGE_ERROR_CODE funcErrCode = E_SUCCESS;
	std::list<FunctionInfo>ResponseFuncionList;

	std::list<FunctionInfo>::const_iterator iterFunc = listFuncReq.begin();
	while(iterFunc != listFuncReq.end())
	{
		DebugPrintf(SV_LOG_DEBUG, "Function Name is in the list %s\n",iterFunc->m_RequestFunctionName.c_str()) ;
		FunctionInfo FunData = *iterFunc;
		ProcessAPI(FunData) ;
		ResponseFuncionList.push_back(FunData);
		iterFunc++;
	}
	listFuncReq.clear();
	listFuncReq = ResponseFuncionList;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return funcErrCode;
}

void RepositoryLock::RequestQuit(bool flag)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DebugPrintf(SV_LOG_DEBUG, "Requesting quit\n") ;
	m_quit = flag ;
}
