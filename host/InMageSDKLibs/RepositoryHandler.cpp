#include <boost/filesystem/operations.hpp>
#include "RepositoryHandler.h"
#include "inmstrcmp.h"
#include "VolumeHandler.h"
#include "inmageex.h"
#include "volumeinfocollector.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/volumeconfig.h"
#include "confengine/auditconfig.h"
#include "confengine/alertconfig.h"
#include "confengine/agentconfig.h"
#include "inmsdkutil.h"
#include "service.h"
#include "confengine/scenarioconfig.h"
#include "confengine/protectionpairconfig.h"
#include "util.h"
#include "service.h"
#include "ProtectionHandler.h"
#include <boost/algorithm/string/replace.hpp>
#include "portablehelpersmajor.h"
#include "inmagesdkutil.h"
#include "paircreationhandler.h"
#include "cdpvolumeutil.h"
#include "appcommand.h"
#include "cdpplatform.h"
#include "applocalconfigurator.h"
#include "host.h"
#include "confschemamgr.h"
#include "APIController.h"
#ifdef SV_WINDOWS
#include "virtualvolume.h"
#include "../drivers/invirvol/win32/VVDevControl.h"
#include "xmlizeinitialsettings.h"
#include "serializeinitialsettings.h"
//#include <windows.h>
#endif
#include <boost/algorithm/string.hpp>
#include "../inmapihandler/confengine/volumeconfig.h"
#include "../inmsafecapis/inmsafecapis.h"

std::string RepositoryHandler::m_domain = "";
std::string RepositoryHandler:: m_username = "";
std::string RepositoryHandler:: m_password = "";

void getprogress(ACE_Time_Value starttime,
				 SV_ULONGLONG totaldata,
				 SV_ULONGLONG copieddata,
				 SV_ULONGLONG& percentagecompleted,
				 SV_ULONGLONG& timelapsed,
				 SV_ULONGLONG& remainingtime)
{
	ACE_Time_Value currtime = ACE_OS::gettimeofday(); ;
	timelapsed = currtime.sec() - starttime.sec() ;
	percentagecompleted = ( copieddata / totaldata ) * 100 ;
	remainingtime = ( ( totaldata - copieddata ) * timelapsed ) / copieddata ;
}

std::string secondsToHrf(SV_ULONGLONG secs)
{
	std::string hours, min, seconds, finalstring ;
	if( secs / ( 60 * 60 ) > 0 )
	{
		hours = boost::lexical_cast<std::string>( secs / ( 60 * 60 ) ) ;
		secs = secs % ( 60 *60 ) ;
	}
	if( secs / 60 > 0 )
	{
		min = boost::lexical_cast<std::string>( secs / 60 ) ;
		secs = secs % 60 ;
	}
	if( secs >= 0 )
	{
		seconds = boost::lexical_cast<std::string>( secs ) ;
	}
	if( !hours.empty() )
	{
		finalstring += hours ;
		finalstring += " hrs " ;
	}
	if( !min.empty() )
	{
		finalstring += min ;
		finalstring += " mins " ;
	}
	if( !seconds.empty() )
	{
		finalstring += seconds ;
		finalstring += " secs " ;
	}
	return finalstring ;
}

bool RepositoryHandler::CopyFile(const std::string & SourceFile, const std::string & DestinationFile, unsigned long& error)
{
	error = 0 ;
	bool rv = true;
	const std::size_t buf_sz = 32768;
	char buf[buf_sz];
	ACE_HANDLE infile=ACE_INVALID_HANDLE, outfile=ACE_INVALID_HANDLE;
	ACE_stat from_stat = {0};
	// PR#10815: Long Path support
	if ( sv_stat(getLongPathName(SourceFile.c_str()).c_str(), &from_stat ) == 0 )
	{
		if( (infile = ACE_OS::open(getLongPathName(SourceFile.c_str()).c_str(), O_RDONLY )) != ACE_INVALID_HANDLE )
		{
			if( (outfile = ACE_OS::open(getLongPathName(DestinationFile.c_str()).c_str(), O_WRONLY | O_CREAT | O_TRUNC )) != ACE_INVALID_HANDLE  )
			{
				error = 0 ;
				ssize_t sz, sz_read=1, sz_write;
				while ( sz_read > 0
					&& (sz_read = ACE_OS::read( infile, buf, buf_sz )) > 0 )
				{
					sz_write = 0;
					do
					{
						if ( (sz = ACE_OS::write( outfile, buf + sz_write, sz_read - sz_write )) < 0 )
						{
							sz_read = sz; // cause read loop termination
							break;        //  and error to be thrown after closes
						}
						sz_write += sz;
					} while ( sz_write < sz_read );
				}
				if ( ACE_OS::close( infile) < 0 ) 
				{
					sz_read = -1;
				}
				infile = ACE_INVALID_HANDLE ;
				if ( ACE_OS::close( outfile) < 0 ) 
				{
					sz_read = -1;
				}
				outfile = ACE_INVALID_HANDLE ;
				if ( sz_read < 0 )
				{
					
					error = ACE_OS::last_error() ;
					DebugPrintf(SV_LOG_ERROR, "Failed to copy %s with error %ld\n", SourceFile.c_str(),
										error) ;
					rv = false;
				}
			}
			else
			{
				rv = false;
				error = ACE_OS::last_error() ;
				DebugPrintf(SV_LOG_ERROR, "Failed to open the file %s with error %ld\n", DestinationFile.c_str(),
					error) ;
			}
		}
		else
		{
			rv = false;
			error = ACE_OS::last_error() ;
			DebugPrintf(SV_LOG_ERROR, "Failed to open the file %s with error %ld\n", SourceFile.c_str(),
				error) ;
		}
	}
	else
	{
		rv = false;
		error = ACE_OS::last_error() ;
		DebugPrintf(SV_LOG_ERROR, "Failed to stat %s with error %ld\n", SourceFile.c_str(),
			error) ;
	}
	if ( outfile != ACE_INVALID_HANDLE) 
		ACE_OS::close( outfile );
	if ( infile != ACE_INVALID_HANDLE) 
		ACE_OS::close( infile );
	return rv;	
}

std::string GetHostNameFromSharePath(const std::string& sharepath)
{
	std::string hostname = sharepath ;
	hostname = hostname.substr(2) ;
	hostname = hostname.substr(0,hostname.find('\\')) ;
	if( hostname.empty() )
	{
		hostname = "." ;
	}
	DebugPrintf(SV_LOG_DEBUG, "hostname %s\n", hostname.c_str()) ;
	return hostname ;
}

RepositoryHandler::RepositoryHandler(void)
{
}
RepositoryHandler::~RepositoryHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
INM_ERROR RepositoryHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	INM_ERROR errCode = E_SUCCESS;
	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(), "ListRepositoryDevices") == 0)
	{
		errCode = ListRepositoryDevices(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListConfiguredRepositories") == 0)
	{
		errCode = ListConfiguredRepositories(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ReconstructRepo") == 0 )
	{
		errCode = ReconstructRepo(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"SetupRepository") == 0)
	{
		errCode = SetupRepository(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"DeleteRepository") == 0)
	{
		errCode = DeleteRepository(request) ;
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"MoveRepository") == 0)
	{
		errCode = MoveRepository(request) ;
	}
	else if( InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"SetCompression") == 0)
	{
		errCode = SetCompression(request) ;
	}
	else if( InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"UpdateCredentials") == 0)
	{
		errCode = UpdateCredentials(request) ;
	}
	else if( InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(), "ChangeBackupLocation") == 0)
	{
		errCode = ChangeBackupLocation(request) ;
	}
	else if( InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(), "ArchiveRepository") == 0)
	{
		errCode = ArchiveRepository(request) ;
	}
	else if( InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(), "ShouldMoveOrArchiveRepository") == 0)
	{
		errCode = ShouldMoveOrArchiveRepository(request) ;
	}
	return Handler::updateErrorCode(errCode);
}
void RepositoryHandler::ReportProgress(std::stringstream& stream, bool isError)
{
	
	std::ofstream progressstream(m_progresslogfile.c_str(), std::ios::app) ;
	if( isError )
	{
		std::cerr<< stream.str() ;
	}
	else
	{
		std::cout<< stream.str() ;
	}
	std::cout.flush() ;
	std::cerr.flush() ;
	progressstream << stream.str() ;
	progressstream.flush() ;
	progressstream.close() ;
	stream.str("") ;
}

INM_ERROR RepositoryHandler::ModifyThePathsIfRequired(const std::string& existingRepo,
													  const std::string& newRepo,
													  LocalConfigurator& configurator)
{
	INM_ERROR errCode = E_INTERNAL ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	DebugPrintf(SV_LOG_DEBUG, "Repo Path in xml store %s, Supplied path %s\n", existingRepo.c_str(), 
		newRepo.c_str()) ;
	std::string configDatPath = constructConfigStoreLocation( getRepositoryPath( newRepo ), configurator.getHostId() ) ;
	configDatPath += "config.dat" ;
	std::string newConfigDat = configDatPath + ".1" ;
	ACE_OS::unlink(newConfigDat.c_str()) ; 
	try
	{
#ifdef SV_WINDOWS
		boost::filesystem::copy_file( configDatPath.c_str(), newConfigDat.c_str()) ;
		std::ofstream of( configDatPath.c_str() ) ;
		if( of.is_open() )
		{
			InitialSettings settings ;
			of<< CxArgObj<InitialSettings>( settings ) ;
			of.flush(); 
			of.close() ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "unable to create the dummy settings file in %s\n",configDatPath.c_str() ); 
		}
#endif	
	}
	catch(boost::filesystem::filesystem_error& fileErr)
	{
		DebugPrintf(SV_LOG_ERROR, "Copying the %s to %s is failed %s\n", configDatPath.c_str(), 
			newConfigDat.c_str()) ;
	}
	if( existingRepo != newRepo )
	{
		volumeConfigPtr->ChangeRepoPath( newRepo ) ;
		protectionPairConfigPtr->ModifyRetentionBaseDir(existingRepo, newRepo) ;
		scenarioConfigPtr->ModifyRetentionBaseDir(existingRepo, newRepo) ;
	}
	else
	{
		errCode = E_SUCCESS ;
		DebugPrintf(SV_LOG_DEBUG, "No entries need to be modifed in configuration store\n") ;
	}
	return errCode ;
}

INM_ERROR RepositoryHandler::ChangeBackupLocation(FunctionInfo& finfo)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator configurator ;
	bool guestAccess = false ;
	std::string repoDeviceName ;
	INM_ERROR errCode = E_SUCCESS ;
	if( finfo.m_RequestPgs.m_Params.find( "GuestAccess" ) != 
			finfo.m_RequestPgs.m_Params.end() )
		{
			if( finfo.m_RequestPgs.m_Params.find( "GuestAccess" )->second.value == "Yes" )
			{
				DebugPrintf(SV_LOG_DEBUG, "Guest Access\n") ;
				guestAccess = true ;
			}
		}
	if( finfo.m_RequestPgs.m_Params.find("RepositoryDeviceName") !=  finfo.m_RequestPgs.m_Params.end() )
	{
		repoDeviceName = finfo.m_RequestPgs.m_Params.find("RepositoryDeviceName")->second.value ;
	}

	if(repoDeviceName.length() <= 3)
	{
		repoDeviceName = repoDeviceName[0];
	}
	else if(repoDeviceName.length() > 3 && 
		repoDeviceName[repoDeviceName.length() - 1] == '\\' )
	{
		repoDeviceName.erase(repoDeviceName.length() - 1) ;
	}
	VolumeSummaries_t volumeSummaries;
	VolumeDynamicInfos_t volumeDynamicInfos;
	VolumeInfoCollector volumeCollector;
	volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, true); 
	VolumeSummaries_t::iterator volSummaryIterB = volumeSummaries.begin() ;
	VolumeSummaries_t::iterator volSummaryIterE = volumeSummaries.end() ;
	while( volSummaryIterB != volSummaryIterE )
	{
		if( volSummaryIterB->name.length() > 1 )
		{
			volSummaryIterB->name.erase(volSummaryIterB->name.length() - 1) ;
		}
		if( InmStrCmp<NoCaseCharCmp>(volSummaryIterB->name, repoDeviceName ) == 0 )
		{
			repoDeviceName = volSummaryIterB->name ;
			break ;
		}
		volSummaryIterB++ ;
	}
	ACE_HANDLE handle ;
	if( !isRepositoryConfiguredOnCIFS( repoDeviceName ) && 
		OpenVolumeExtended( &handle, repoDeviceName.c_str(), GENERIC_READ ) != SVS_OK )
	{
		errCode = E_STORAGE_PATH_NOT_FOUND;
	}
	if( errCode == E_SUCCESS )
	{
		LocalConfigurator lc ;
		std::string username, password, domain ;
		if( isRepositoryConfiguredOnCIFS( repoDeviceName ) )
		{
			std::string errmsg ;
			if( !guestAccess )
			{
				GetCredentialsFromRequest(finfo, username, password, domain, errmsg) ;
			}
			AccessRemoteShare(getRepositoryPath(repoDeviceName), username, password, domain, guestAccess, errCode, errmsg) ;
			if(  errCode == E_SUCCESS )
			{
				/*SaveCredentialsInReg(username, password, domain, errCode, errmsg ) ;*/				
				persistAccessCredentials(getRepositoryPath(repoDeviceName),username, password, 
										domain, errCode, errmsg, guestAccess);

			}
			if( errCode != E_SUCCESS )
			{
				finfo.m_Message = errmsg ;
			}
		}
		if( errCode == E_SUCCESS )
		{
			std::string newRepoPathForHost = constructConfigStoreLocation( getRepositoryPath(repoDeviceName), lc.getHostId() ) ;
			RepositoryLock repoLock(getLockPath(newRepoPathForHost)) ;
			repoLock.Acquire() ;
			ConfSchemaReaderWriter::CreateInstance( "ChangeBackupLocation", getRepositoryPath(repoDeviceName), newRepoPathForHost , false ) ;
			VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
			if( volumeConfigPtr->IsValidRepository() )
			{
				std::string repopathInVolumeConfig, reponame ;
				volumeConfigPtr->GetRepositoryNameAndPath(reponame, repopathInVolumeConfig) ;
				ModifyThePathsIfRequired(repopathInVolumeConfig, repoDeviceName, lc) ;
				std::string repoPathInDrscoutConf = configurator.getRepositoryLocation() ;
				configurator.setRepositoryLocation(repoDeviceName)  ;
				if( ReMountVolpacks( configurator, repoPathInDrscoutConf, repoDeviceName, true ) )
				{
					ConfSchemaReaderWriterPtr confschemardrwrtr = ConfSchemaReaderWriter::GetInstance() ;
					confschemardrwrtr->Sync() ;
				}
				else
				{
					errCode = E_INTERNAL ;
					finfo.m_Message = "Failed to re-mount the target volume" ;
					DebugPrintf(SV_LOG_ERROR, "Failed to Remount the target volumes\n"); 
				}
			}
			else
			{
				errCode = E_INVALID_REPOSITORY ;
			}
		}
	}
	StartSvc("backupagent") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

/*
function returns true if Backuplocation and InstallationVolume are not same 
other wise return false
*/
bool RepositoryHandler::isBackuplocationAndInstallationVolumeSame (const std::string &newRepoPath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;

	bool bRet = true;
#ifdef SV_WINDOWS 
	std::string destinationVolume = newRepoPath ; 
	std::stringstream stream;
	LocalConfigurator localconfigurator;
	std::string instalPath = localconfigurator.getInstallPath(); 
	char  installVolume[MAX_PATH + 1] = {0};
	if (instalPath [ instalPath.length () - 1 ]   != '\\' )
	{
		instalPath += "\\"; 
	}
	DebugPrintf (SV_LOG_DEBUG, "instalPath is  %s \n ", instalPath.c_str()); 
	if (! SVGetVolumePathName ( ( LPCTSTR) instalPath.c_str(),(LPTSTR) installVolume, MAX_PATH)) 
	{
		DebugPrintf ( SV_LOG_ERROR, "GetLast Error is %d \n ",ACE_OS::last_error() );
	}		
	if (destinationVolume.length () == 1 )
	{
		destinationVolume += ":\\"; 
	}
	else 
	{
		destinationVolume += "\\"; 
	}

	if (destinationVolume == installVolume)
	{
		bRet = false; 
		stream << "The destination should not be installation volume.\n Cannot proceed further..." << std::endl; 
		ReportProgress(stream) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
#endif
	return bRet; 
}

bool RepositoryHandler::IsVolumdProvisioningUnProvisioningInProgress()
{
	bool bRet = true ; 
	std::list <std::string> backupRepositoriesVolumes; 
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	std::list<std::string> protectedVolumes ;
	scenarioConfigPtr->GetProtectedVolumes(protectedVolumes) ;
	std::list<std::string>::const_iterator volIter = protectedVolumes.begin() ;
	while( volIter != protectedVolumes.end() )
	{
		std::string volumeprovisioningStatus ;
		scenarioConfigPtr->GetVolumeProvisioningStatus(*volIter, volumeprovisioningStatus) ;
		if( volumeprovisioningStatus != VOL_PACK_SUCCESS )
		{
			bRet = false ;
		}
		volIter++ ;
	}
	return bRet; 
}
bool RepositoryHandler::isSameRepoOrNot(std::string& repoPath, std::string& newRepoPath)
{
			bool bRet = false ;
			std::stringstream stream ;
			std::string repoPathForTmpfile = repoPath, newRepoPathForTmpfile = newRepoPath ;

			time_t ltime;
			struct tm *today;

			time( &ltime );
			today = gmtime( &ltime );

			char szGmtTimeBuff[100]; /* To hold the time in  buffer form */

			// initialize 100 bytes of the character string to 0
			memset(szGmtTimeBuff,0,100);

			inm_sprintf_s(szGmtTimeBuff, ARRAYSIZE(szGmtTimeBuff), "(20%02d-%02d-%02d %02d:%02d:%02d):",
						today->tm_year - 100,
						today->tm_mon + 1,
						today->tm_mday,		
						today->tm_hour,
						today->tm_min,
						today->tm_sec
					);
			std::string tmpFile(szGmtTimeBuff); 
			tmpFile+= ".txt" ;
			tmpFile.erase (std::remove(tmpFile.begin(),tmpFile.end(), ':' ), tmpFile.end());

			if( repoPathForTmpfile.length() == 1 )
			{
				repoPathForTmpfile += ":\\" ;
			}
			if( repoPathForTmpfile[repoPathForTmpfile.length() - 1 ] != '\\' )
			{
				repoPathForTmpfile += '\\' ;
			}
			if( newRepoPathForTmpfile.length() == 1 )
			{
				newRepoPathForTmpfile += ":\\" ;
			}
			if( newRepoPathForTmpfile[newRepoPathForTmpfile.length() - 1 ] != '\\' )
			{
				newRepoPathForTmpfile += '\\' ;
			}
			
			repoPathForTmpfile += "tmp_" + tmpFile ; 
			newRepoPathForTmpfile += "tmp_" + tmpFile ;
#ifdef SV_WINDOWS

			HANDLE tmpFileHandle = INVALID_HANDLE_VALUE ;
			
			//tmpFileHandle = ::CreateFile(repoPathForTmpfile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			tmpFileHandle = ACE_OS::open(repoPathForTmpfile.c_str(),O_WRONLY | O_CREAT | O_EXCL ,FILE_SHARE_WRITE );
			if (tmpFileHandle == INVALID_HANDLE_VALUE) 
					{
						DebugPrintf(SV_LOG_ERROR, "Unable to access the backup location  %s  %s\n", repoPath.c_str(),
																 Error::Msg().c_str() ) ;
						
					}
			struct stat stFileInfo ;
			int intStat ;
			intStat = stat( newRepoPathForTmpfile.c_str(),&stFileInfo );
			if ( intStat == 0  )
			{
				bRet = true ;
			}
			ACE_OS::close(tmpFileHandle);
			ACE_OS::unlink( repoPathForTmpfile.c_str()) ;
#endif
		return bRet ;
}

INM_ERROR RepositoryHandler::ShouldMoveOrArchiveRepository(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ConfSchemaReaderWriterPtr confschemardrwrtr;
	std::string progresslogfile ;
	std::string newRepoPath ;
	LocalConfigurator lc ;
	m_progresslogfile = lc.getInstallPath() ;
	m_progresslogfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	m_progresslogfile += "bxadmin_progress.log" ;
	if( request.m_RequestPgs.m_Params.find( "NewRepositoryDeviceName" ) !=
		request.m_RequestPgs.m_Params.end() )
	{
		newRepoPath = request.m_RequestPgs.m_Params.find( "NewRepositoryDeviceName" )->second.value ;
	}
	if( isRepositoryConfiguredOnCIFS( newRepoPath ) && newRepoPath[newRepoPath.length() - 1 ] == '\\' )
	{
		newRepoPath.erase( newRepoPath.length() - 1 ) ;
	}
	std::string errMsg ;
	std::string username, password ;
	bool guestAccess = false ;
	if( request.m_RequestPgs.m_Params.find( "GuestAccess" ) != 
			request.m_RequestPgs.m_Params.end() )
		{
			if( request.m_RequestPgs.m_Params.find( "GuestAccess" )->second.value == "Yes" )
			{
				DebugPrintf(SV_LOG_DEBUG, "Guest Access\n") ;
				guestAccess = true ;
			}
		}
	if ( !guestAccess )
	{
		if(request.m_RequestPgs.m_Params.find( "UserName" ) != request.m_RequestPgs.m_Params.end() )
		{
			username = request.m_RequestPgs.m_Params.find( "UserName" )->second.value ;
		}
		if( request.m_RequestPgs.m_Params.find( "PassWord" ) != request.m_RequestPgs.m_Params.end() )
		{
			password = request.m_RequestPgs.m_Params.find( "PassWord" )->second.value ;
		}
	}
	do
	{
		std::string repoName,  repoPath ;
		LocalConfigurator lc ;
		RepositoryLock repoLock( getLockPath(constructConfigStoreLocation(getRepositoryPath(lc.getRepositoryLocation()), lc.getHostId()))) ;
		repoLock.Acquire() ;
		std::stringstream stream ;
		bool IsRecoverablecorruption = ConfSchemaReaderWriter::ActOnCorruptedGroups();
		if( !IsRecoverablecorruption )
		{
			stream<<" The Configuration store is not in consistent state.Please rebuild the configstore using bxadmin tool and try again.."<< std::endl ;
			ReportProgress(stream) ;
			break ;
		}
		ConfSchemaReaderWriter::CreateInstance( "MoveRepository", getRepositoryPath(lc.getRepositoryLocation()), constructConfigStoreLocation(getRepositoryPath(lc.getRepositoryLocation()), lc.getHostId()) , false) ;
		ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
		VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
		std::map<std::string, volumeProperties> volumePropertiesMap ;
		volumeProperties  newRepoDeviceProperties ;
		volumeConfigPtr->GetRepositoryNameAndPath( repoName, repoPath ) ;
		std::list<std::string> protectedVolumes ;
		scenarioConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
		stream<< "\nExisting backup location device "<< repoPath << std::endl ;
		ReportProgress(stream) ;
		stream<<"Do not reboot the server until the operation completed\n" ;
		ReportProgress(stream) ;
		if( !scenarioConfigPtr->ProtectionScenarioAvailable() )
		{
			stream<<"No Protections Found. Exiting.."<< std::endl ;
			ReportProgress(stream) ;
			break ;
		}
		if( !volumeConfigPtr->GetVolumeProperties( volumePropertiesMap ) )
		{
			stream<<"Cannot find the volume properties of the system. Exiting.."<< std::endl ;
			ReportProgress(stream) ;
			break ;
		}
		if ( IsVolumdProvisioningUnProvisioningInProgress() == false )
		{
			stream<< "Preparing/deleting the backup media is in progress " << std::endl;
			ReportProgress(stream) ;
			stream<< "Cannot continue further. Please try again after some time. Exiting.." << std::endl ;
			ReportProgress(stream) ;
			break; 
		}
		if( !isRepositoryConfiguredOnCIFS( newRepoPath ) )
		{
			if( newRepoPath.length() > 3 )
			{
				std::string newRepoVolumeName,copyOfnewRepoPath ;
				copyOfnewRepoPath = newRepoPath ;
				std::list<std::string> volumes ;
				bool isContainMountFolder = false ;
				scenarioConfigPtr->GetProtectedVolumes( volumes );
				std::list<std::string>::const_iterator volIter = volumes.begin() ;
				size_t countSlash = std::count(copyOfnewRepoPath.begin(), copyOfnewRepoPath.end(), '\\') ;
				if ( volIter != volumes.end() )
				{	
					while ( countSlash > 1 )
					{
						newRepoVolumeName = copyOfnewRepoPath.substr( 0 , copyOfnewRepoPath.find_last_of("\\") ) ;
						if( std::find( volumes.begin(), volumes.end(), newRepoVolumeName ) != 
							volumes.end())
						{
							isContainMountFolder = true ;
							break ;
						}
						countSlash--;
						copyOfnewRepoPath = copyOfnewRepoPath.substr( 0 , copyOfnewRepoPath.find_last_of("\\") ) ;

					}
					if( countSlash == 1 )
					{
						newRepoVolumeName = copyOfnewRepoPath[0] ;
						if( std::find( volumes.begin(), volumes.end(), newRepoVolumeName ) != 
							volumes.end())
						{
							isContainMountFolder = true ;
						}
					}
					if ( isContainMountFolder )
					{
						stream<<"Cannot Archive/Move backuplocation to a mount point whose mount folder is with in a protected volume or mount point. Exiting.."<< std::endl ;
						ReportProgress(stream) ;
						break ;
					}
				}
			}
		}
		
	
		repoLock.Release() ;
#ifdef SV_WINDOWS
		initConfig() ;
		VsnapMgr *Vsnap;
        WinVsnapMgr vsnapshot;
        Vsnap=&vsnapshot;
		std::vector<VsnapPersistInfo> vsnaps;
		 if(Vsnap->RetrieveVsnapInfo(vsnaps,  "all") )
		 {
			 if( !vsnaps.empty() )
			 {
				 stream << "Unmount the existing vsnaps and retry the operation.. Exiting.. " <<std::endl ;
				 ReportProgress(stream) ;
				 break ;
			 }
		 }
#endif
		if (isBackuplocationAndInstallationVolumeSame(newRepoPath) == false )
		{	
			break; 
		}
		if( !isRepositoryConfiguredOnCIFS( newRepoPath ) )
		{
			if( volumePropertiesMap.find( newRepoPath ) == volumePropertiesMap.end() || 
				volumePropertiesMap.find( newRepoPath )->second.VolumeType != "5" )
			{
				stream<<"Chosen backup location device " << newRepoPath << " doesn't exist on the system." << std::endl ;
				stream<<"Choose the device that exists on the system and try again. Exiting.." << std::endl ;
				ReportProgress(stream) ;
				break ;
			}
			newRepoDeviceProperties = volumePropertiesMap.find( newRepoPath )->second ;
			if( ! (newRepoDeviceProperties.fileSystemType.compare("NTFS") == 0 || 
				   newRepoDeviceProperties.fileSystemType.compare("ReFS") == 0 || 
				   newRepoDeviceProperties.fileSystemType.compare("ExtendedFAT") == 0 ) )
			{
				stream<<"The device " << newRepoPath << " should have ntfs file system on it. " << std::endl;
				stream<<"Choose device with ntfs file system on it and try again. Exiting.." << std::endl ;
				ReportProgress(stream) ;
				break ;
			}
			if( newRepoDeviceProperties.isBootVolume.compare("1") == 0 || 
				newRepoDeviceProperties.isSystemVolume.compare("1") == 0 )
			{
				stream<<"The backup location device should not be boot/system volume" << std::endl;
				stream<<"Choose non-boot/non-system volume and try again. Exiting" << std::endl ;
				ReportProgress(stream) ;
				break ;
			}
#ifdef SV_WINDOWS
			SV_UINT physicalSecSize ;
			cdp_volume_util u;
			if( u.GetPhysicalSectorSize( newRepoDeviceProperties.mountPoint, physicalSecSize) )
			{
				if( physicalSecSize != 512 )
				{
					//errCode = E_REPO_UNSUPPORTED_SECTORSIZE ;
					std::string errMessage ;
					errMessage = "Physical sector size of ";
					errMessage += newRepoDeviceProperties.mountPoint ;
					errMessage += " is " ;
					errMessage += boost::lexical_cast<std::string>(physicalSecSize) ;
					errMessage += ". This is not supported. Make sure to use any disk with 512 byte sector." ;
					stream << errMessage << std::endl ;
					ReportProgress(stream) ;
					break ;				
				}
			}
			else
			{
				stream<<std::endl ;
				ReportProgress(stream) ;
				break ;
			}
#endif
			if( std::find( protectedVolumes.begin(), protectedVolumes.end(), newRepoPath )
				!= protectedVolumes.end() )
			{
				stream<<"Cannot use protected drive as backup location device.. " << std::endl ;
				stream<<"Choose device that is not protected and try again.. Exiting.." << std::endl; 
				ReportProgress(stream) ;
				break ;
			}
			if( isSameRepoOrNot(repoPath,newRepoPath) )
			{
				stream<<"The existing backup location path and new backup location should not be same." << std::endl ;
				stream<<"Choose another device and try again.. Exiting.." << std::endl ;
				ReportProgress(stream) ;
				break ;
			}
			if( newRepoDeviceProperties.writeCacheState.compare("1") != 0 )
			{
				stream<<"Disk caching status is "  ;
				if( newRepoDeviceProperties.writeCacheState.compare("0") == 0 )
				{
					stream<<"unknown " ;
				}
				else
				{
					stream<<"enabled " ;
				}
				stream << "for the disk on which " << newRepoPath ;
				stream << " exists. Disk caching should be disabled as it may cause backup location corruption in case of a unplanned shutdown of the system" ;
				stream << std::endl ;
				ReportProgress(stream) ;
			}
			std::string volume ;
			std::string diskname ;
			if( DoesDiskContainsProtectedVolumes( newRepoPath, protectedVolumes, diskname, volume ) )
			{
				if( diskname != "__INMAGE__DYNAMIC__DG__" )
				{
					stream<<"The chosen backup location " << newRepoPath << " and proteced volume "<< volume << " are on same disk " << diskname << std::endl ;
				}
				else
				{
					stream<<"The chosen backup location " << newRepoPath << " and proteced volume "<< volume << " are on same dynamic disk group " <<std::endl ;
				}
				stream<<"Please choose the disk which doesn't have any protected volumes in it.."<<std::endl ;
				ReportProgress(stream) ;
				break ;
			}
		}
		else 
		{
			std::string newRepoPathCheck = newRepoPath ; 
			if (newRepoPathCheck[newRepoPathCheck.length () - 1 ] == '\\')
			{
				newRepoPathCheck = newRepoPathCheck.substr (0,newRepoPathCheck.length () - 1); 
			}
			
			if ( !guestAccess)
			{
				std::string fqdnusername ;
				if( !username.empty() )
				{
					fqdnusername = username ;
				}
				else
				{
				std::cout<<"Provide the access credentials for " << newRepoPath << std::endl ;
				std::cout<<"Enter User Name <Domain Name\\User Name> :" ;
				std::cin >> fqdnusername ;
				std::cout<< std::endl ;
				std::cout<<"Enter Password :" ;
				std::cin >> m_password ;
				}
				if( !password.empty() )
				{
					m_password = password ;
				}
				m_username = fqdnusername; 
				if( fqdnusername.find('\\') != std::string::npos )
				{
					m_username = fqdnusername.substr(fqdnusername.find('\\') + 1 ) ;
					m_domain = fqdnusername.substr(0, fqdnusername.find('\\')) ;
				}
				if( m_domain.empty() || m_domain == "." )
				{
					m_domain = GetHostNameFromSharePath( newRepoPath ) ;
				}
			}
			std::string errormsgForNewRepo,errormsgForOldRepo ;
			std::string usernameForOld, domainForOld, passwordForOld ;
			INM_ERROR errCodeForNewRepo,errCodeForOldRepo ;
			LocalConfigurator lc ;
			bool guestAccessForOldRepo = lc.isGuestAccess();
			if( isRepositoryConfiguredOnCIFS( repoPath ) && !guestAccessForOldRepo    )
				{
					if(  CredentailsAvailable() )
					{
						GetCredentials(domainForOld, usernameForOld, passwordForOld ) ;
					}
				}
			AccessRemoteShare(getRepositoryPath(repoPath), usernameForOld, passwordForOld, domainForOld, guestAccessForOldRepo, errCodeForOldRepo, errormsgForOldRepo,true);
			AccessRemoteShare(getRepositoryPath(newRepoPath), m_username, m_password, m_domain, guestAccess, errCodeForNewRepo, errormsgForNewRepo,true) ; 
			bool accessForBoth = true ;
			if( errCodeForNewRepo != E_SUCCESS )
			{
				accessForBoth = false ;
				stream<< "Unable to access " << newRepoPath << ". " << errormsgForNewRepo << std::endl; 
				stream<<"Exiting..\n" ;
				ReportProgress(stream) ;
				break ;
			}
			if(errCodeForOldRepo != E_SUCCESS)
			{
				accessForBoth = false ;
				stream<< "Unable to access " << repoPath << ". " << errormsgForOldRepo << std::endl; 
				stream<<"Exiting..\n" ;
				ReportProgress(stream) ;
				break ;
			}
			if(accessForBoth)
			{
				if( isSameRepoOrNot(repoPath,newRepoPathCheck) )
				{
					stream<<"The existing backup location path and given backup location should not be same." << std::endl ;
					stream<<"Choose another device and try again.. Exiting.." << std::endl ;
					ReportProgress(stream) ;
					break ;
				}
			}
		}
		repoLock.Acquire() ;
		ConfSchemaReaderWriter::CreateInstance( "MoveRepository", getRepositoryPath(lc.getRepositoryLocation()), constructConfigStoreLocation(getRepositoryPath(lc.getRepositoryLocation()), lc.getHostId()) , false) ;
		scenarioConfigPtr = ScenarioConfig::GetInstance() ;
		if( scenarioConfigPtr->ProtectionScenarioAvailable() )
		{
			stream<<"Identified existing protections in backup location" << std::endl;
			ReportProgress(stream) ;
			RetentionPolicy retentionPolicy ;
			if( scenarioConfigPtr->GetRetentionPolicy( "", retentionPolicy )  )
			{
				std::map<std::string, PairDetails> pairDetailsMap;
				scenarioConfigPtr->GetPairDetails(pairDetailsMap) ;
				SV_ULONGLONG requiredSpace = 0 ;
				SV_ULONGLONG repoFreeSpace = 0 ;
				SV_ULONGLONG protectedVolumeOverHead = 0;
				std::string overRideSpaceCheck ; 
				isSpaceCheckRequired (request , overRideSpaceCheck);
				bool bSpaceAvailability = false ; 
				if (overRideSpaceCheck != "Yes")
				{
					spaceCheckParameters spaceParameter ; 
					ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
					scenarioConfigPtr->GetIOParameters (spaceParameter); 
					double compressionBenifits = spaceParameter.compressionBenifits; 
					double sparseSavingsFactor = 3.0; 
					double changeRate = spaceParameter.changeRate ;
					double ntfsMetaDataoverHead = 0.13; 
					std::string testRepoVolName ;
					//logic to test the new repo supports sparse file or not
					bool supportSparseFile = false ;
					if ( SupportsSparseFiles(newRepoPath) )
						supportSparseFile = true;
					SV_ULONGLONG filecount, size_on_disk , logicalsize=0, total_size_on_disk=0 ;
					filecount = size_on_disk = logicalsize = 0 ;
					AgentConfigPtr agentConf = AgentConfig::GetInstance() ;
					std::string  hostId ,testRepoVol ;
					hostId = agentConf -> getHostId();
					testRepoVol = repoPath ;
					testRepoVol = getRepositoryPath(testRepoVol) + hostId ;
					std::string filePattern = "*" ;
					DebugPrintf(SV_LOG_DEBUG, "Getting used space for path is %s\n", testRepoVol.c_str()) ;
					stream<< "Calculating the space used by current backup location " << repoPath << std::endl ;
					ReportProgress(stream) ;
					GetUsedSpace(testRepoVol.c_str(), filePattern.c_str() ,filecount, total_size_on_disk, logicalsize) ;
					if (isRepositoryConfiguredOnCIFS (newRepoPath) == false)
					{
						repoFreeSpace = boost::lexical_cast<SV_ULONGLONG>(newRepoDeviceProperties.freeSpace) ;
					}
					else 
					{
						SV_ULONGLONG capacity,quota;
						GetDiskFreeSpaceP (newRepoPath ,&quota ,&capacity,&repoFreeSpace);
					}
					DebugPrintf (SV_LOG_DEBUG , "repoFreeSpace is %llu\n" ,repoFreeSpace );
					if (supportSparseFile)
					{
						requiredSpace = total_size_on_disk ;
						if ( total_size_on_disk < repoFreeSpace )
						{
							bSpaceAvailability = true ;
						}
					}
					else
					{
						requiredSpace = logicalsize ;
						if ( logicalsize < repoFreeSpace )
						{
							bSpaceAvailability = true ;
						}
					}
					stream << "Backup location free space : " << convertToHumanReadableFormat (repoFreeSpace)<< ". Required space : " << convertToHumanReadableFormat (requiredSpace) << " ( approximately )" ;
					ReportProgress(stream,false) ;
					if( !bSpaceAvailability )
					{
						stream<< "Try again with backup location of enough free space.. Exiting.." << std::endl ;
						ReportProgress(stream) ;
						break ;
					}
				}
			}
		}
		errCode = E_SUCCESS ;
	} while( 0 ) ;
	return errCode ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void RepositoryHandler::DeleteOldRepositoryContents( std::string& oldRepoPath, bool stopFiltering)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	if( oldRepoPath[oldRepoPath.length() - 1 ] ==
		ACE_DIRECTORY_SEPARATOR_CHAR )
	{
		oldRepoPath.erase( oldRepoPath.length() - 1 ) ;
	}
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
#ifdef SV_WINDOWS
	if( stopFiltering )
	{
		std::list<std::string> protectedVols ;
		protectionPairConfigPtr->GetProtectedVolumes( protectedVols ) ;
		std::list<std::string>::const_iterator protectedVolIter ;
		protectedVolIter = protectedVols.begin() ;
		LocalConfigurator lc ;
		while( protectedVolIter != protectedVols.end() )
		{
			SV_ULONG dwExitCode = 0 ;
			std::string logfile ;
			std::string stdOutput ;
			bool bProcessActive = true;

			std::string stopfscmdstr = lc.getInstallPath() ;
			stopfscmdstr += ACE_DIRECTORY_SEPARATOR_CHAR ;
			stopfscmdstr += "drvutil.exe" ;
			stopfscmdstr +=  " --StopFiltering " ;
			stopfscmdstr +=  *protectedVolIter ;
			logfile = lc.getInstallPath() ;
			logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
			logfile += "stopFiltering.log" ;
			AppCommand stopfilteringcmd (stopfscmdstr, 0, logfile) ;
			stdOutput = "" ;

			stopfilteringcmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
			protectedVolIter++ ;
		}
	}
#endif
	CDPUtil::CleanAllDirectoryContent(oldRepoPath) ;
	RemoveHostIDEntry() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

INM_ERROR RepositoryHandler::SetCompression(FunctionInfo& request)
{
	bool compression = true ;
	LocalConfigurator configurator ;
	if( request.m_RequestPgs.m_Params.find( "EnableCompression" ) != request.m_RequestPgs.m_Params.end() && 
		request.m_RequestPgs.m_Params.find( "EnableCompression" )->second.value != "Yes" )
	{
		compression = false ;
	}

	configurator.SetCDPCompression( true ) ;
	configurator.SetVirtualVolumeCompression( compression ) ;
	StartSvc("backupagent") ;
	return E_SUCCESS ;
}
bool RepositoryHandler::ReMountVolpacks(LocalConfigurator& configurator,
										const std::string& existingRepo, 
										const std::string& newRepo, bool force )
{
	std::string existingRepoDrive, existingRepoPath, newRepoDrive, newRepoPath ;
	existingRepoDrive = existingRepo ;
	existingRepoDrive = existingRepo ;
	existingRepoPath = existingRepoDrive ;
	if( existingRepo.length() == 1 )
	{
		existingRepoPath += ":\\" ;
	}
	newRepoDrive = newRepo ;
	newRepoPath = newRepo ;
	if( newRepo.length() == 1 )
	{
		newRepoPath += ":\\" ;
	}
	if( newRepoPath[newRepoPath.length() - 1 ] != '\\' )
	{
		newRepoPath += '\\' ;
	}
	if( existingRepoPath[existingRepoPath.length() - 1 ] != '\\' )
	{
		existingRepoPath += '\\' ;
	}
	bool ret = true ;
	char regName[26];
	int counter=configurator.getVirtualVolumesId();
	while(counter!=0)
	{
		inm_sprintf_s(regName, ARRAYSIZE(regName), "VirVolume%d", counter);

		std::string data = configurator.getVirtualVolumesPath(regName);
		std::string sparsefilename;
		std::string volume;

		if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
		{
			counter--;
			continue;
		}
		if( sparsefilename.find(existingRepoPath) != 0 )
		{
			DebugPrintf(SV_LOG_DEBUG, "sparse file path %s, skipping this entry for modification\n", sparsefilename.c_str());
			counter-- ;
			continue ;
		}
		DebugPrintf(SV_LOG_DEBUG, "Current sparsefile %s\n", sparsefilename.c_str()) ;

		sparsefilename = newRepoPath + sparsefilename.substr( existingRepoPath.length() ) ;
		std::string new_value="";
		configurator.setVirtualVolumesPath(regName,new_value);

		std::string mountCmd ="\"" ;
		mountCmd += configurator.getInstallPath() ;
		mountCmd += ACE_DIRECTORY_SEPARATOR_CHAR ;
		mountCmd += "cdpcli.exe\"" ;
		mountCmd += "  --virtualvolume --op=mount" ;
		mountCmd += " --filepath=" ; 
		mountCmd += "\"";
		mountCmd += sparsefilename ; 
		mountCmd += "\""; 
		mountCmd += " --drivename=" ;
		mountCmd += volume ;
		AppCommand appMntCmd(mountCmd, 100) ;
		std::string op ;
		bool quitRequested = true ;
		SV_ULONG exitCode = 0xDEADBEEF ;
		appMntCmd.Run(exitCode, op, quitRequested ) ;
		if( exitCode != 0 )
		{
			ret = false ;
			DebugPrintf(SV_LOG_ERROR, "mounting the %s is failed\n", sparsefilename.c_str()) ;
			break ;
		}		
		counter--;
	}
	return ret  ;
}


void RepositoryHandler::ModifyConfigurationContents(const std::string& existingRepo, 
													const std::string& newRepo )
{
	LocalConfigurator configurator ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	volumeConfigPtr->ChangeRepoPath(newRepo) ;
	scenarioConfigPtr->ModifyRetentionBaseDir( existingRepo, newRepo ) ;
	protectionPairConfigPtr->ModifyRetentionBaseDir( existingRepo, newRepo ) ;
	protectionPairConfigPtr->ResumeProtection() ;
	std::string pausestate = "0" ;
	scenarioConfigPtr->SetSystemPausedState(pausestate) ;
	std::string pausereason = "" ;
	scenarioConfigPtr->SetSystemPausedReason(pausereason) ;
	ConfSchemaReaderWriter::GetInstance()->Sync() ;
}

void RepositoryHandler::RemCredentialsFromRegistry()
{
#ifdef SV_WINDOWS
	::RemCredentialsFromRegistry() ;
#endif
}

INM_ERROR RepositoryHandler::ArchiveRepository(const std::string& newRepoPath, 
											std::string& errMsg)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS ;
	std::stringstream stream ;
	LocalConfigurator lc ;
	std::string repoPath = lc.getRepositoryLocation() ;
	stream<<"Backups will now be Archived to the chosen location" << std::endl ;
	ReportProgress(stream) ;
	std::string repoPathForHost, newRepoPathForHost ;
	LocalConfigurator configurator ;
	repoPathForHost = constructConfigStoreLocation( ConfSchemaMgr::GetInstance()->getRepoLocation(),  lc.getHostId()) ;
	newRepoPathForHost = constructConfigStoreLocation (getRepositoryPath(newRepoPath), lc.getHostId()); 
	//std::string fqdnusername, password, username, domain ;
	std::string errmsg ;
	try
	{
		if( errCode == E_SUCCESS )
		{
			if( CopyRepositoryContents( repoPath, newRepoPath ) )
			{
				stream<<"Archiving the configuration files.."<< std::endl ;
				ReportProgress(stream) ;
				if( CopyConfigurationFiles( repoPath, newRepoPath ) )
				{
					std::string newRepoPathForHost = constructConfigStoreLocation (getRepositoryPath(newRepoPath), configurator.getHostId()); 
					std::string lockPath = getLockPath(newRepoPathForHost) ;
					RepositoryLock repoLock(lockPath) ;
					repoLock.Acquire() ;
					ConfSchemaReaderWriter::CreateInstance( "ArchiveRepository", getRepositoryPath(newRepoPath), newRepoPathForHost , false) ;
					ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
					scenarioConfigPtr->SetBatchSize(0) ;
					ModifyConfigurationContents( repoPath, newRepoPath ) ;
					AddHostDetailsToRepositoryConf(newRepoPath) ;
				}
				else
				{
					errCode = E_INTERNAL ;
				}
			}
			else
			{
				errCode = E_INTERNAL ;
			}
		}
	}
	catch( std::exception& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Archiving the backup location contents from %s to %s failed. %s\n",
			repoPath.c_str(), newRepoPath.c_str(), ex.what()) ;
		errCode = E_INTERNAL ;
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR, "Archiving the backup location contents from %s to %s failed with unknown exception\n",
			repoPath.c_str(), newRepoPath.c_str()) ;
		errCode = E_INTERNAL ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR RepositoryHandler::MoveRepository(const std::string& newRepoPath, 
											std::string& errMsg, bool guestAccess)
{
	INM_ERROR errCode = E_SUCCESS ;
	std::stringstream stream;
	LocalConfigurator lc ;
	std::string repoPath = lc.getRepositoryLocation() ;
	stream<<"Backups will now be moved to the chosen location." << std::endl ;
	ReportProgress(stream) ;
	std::string repoPathForHost, newRepoPathForHost ;
	LocalConfigurator configurator ;
	repoPathForHost = constructConfigStoreLocation( ConfSchemaMgr::GetInstance()->getRepoLocation(),  lc.getHostId()) ;
	newRepoPathForHost = constructConfigStoreLocation (getRepositoryPath(newRepoPath), lc.getHostId()); 
	std::string errmsg ;
	if( errCode == E_SUCCESS )
	{
		do
		{
			try
			{
				if( !CopyRepositoryContents( repoPath, newRepoPath ) )
				{
					errCode = E_INTERNAL ;
					break ;
				}
				std::string lockPathNewRepo = getLockPath(newRepoPathForHost) ;
				std::string lockPathRepo = getLockPath(repoPathForHost) ;
				RepositoryLock repolock(lockPathNewRepo) ;
				repolock.Acquire() ;
				RepositoryLock newRepolock(lockPathRepo) ;
				newRepolock.Acquire() ;
				stream<<"Copying the configuration files.."<< std::endl ;
				ReportProgress(stream) ;
				if( !CopyConfigurationFiles( repoPath, newRepoPath ) )
				{
					errCode = E_INTERNAL ;
					break ;
				}
				ConfSchemaReaderWriter::CreateInstance( "MoveRepository", getRepositoryPath(newRepoPath), newRepoPathForHost, false ) ;
				ModifyConfigurationContents( repoPath, newRepoPath ) ;
				ReMountVolpacks( configurator, repoPath, newRepoPath ) ;
				stream<<"Removing the old backup location contents" << std::endl ;
				ReportProgress(stream) ;
				DeleteOldRepositoryContents(repoPathForHost, false) ;
				stream<<"Removing the old backup location contents successful" << std::endl ;
				ReportProgress(stream) ;
				persistAccessCredentials(newRepoPath,m_username, m_password, m_domain,errCode,errmsg,guestAccess);
				configurator.setRepositoryLocation(newRepoPath) ;
				AddHostDetailsToRepositoryConf(newRepoPath) ;
			}
			catch( std::exception& ex)
			{
				DebugPrintf(SV_LOG_ERROR, "Moving the backup location contents from %s to %s failed. %s\n",
					repoPath.c_str(), newRepoPath.c_str(), ex.what()) ;
				errCode = E_INTERNAL ;
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR, "Moving the backup location contents from %s to %s failed with unknown exception\n",
					repoPath.c_str(), newRepoPath.c_str()) ;
				errCode = E_INTERNAL ;
			}

		} while( false ) ;
	}
	return errCode ;
}

INM_ERROR RepositoryHandler::ArchiveRepository(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator lc ;
	m_progresslogfile = lc.getInstallPath() ;
	m_progresslogfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	m_progresslogfile += "bxadmin_progress.log" ;
	INM_ERROR errCode = E_UNKNOWN ;
	std::string newRepoPath ;
	if( request.m_RequestPgs.m_Params.find( "NewRepositoryDeviceName" ) !=
		request.m_RequestPgs.m_Params.end() )
	{
		newRepoPath = request.m_RequestPgs.m_Params.find( "NewRepositoryDeviceName" )->second.value ;
	}
	if( isRepositoryConfiguredOnCIFS( newRepoPath ) && newRepoPath[newRepoPath.length() - 1 ] == '\\' )
	{
		newRepoPath.erase( newRepoPath.length() - 1 ) ;
	}
	std::string errMsg ;
	errCode = ArchiveRepository(newRepoPath, errMsg) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR RepositoryHandler::MoveRepository(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ConfSchemaReaderWriterPtr confschemardrwrtr;
	std::string progresslogfile ;
	std::string newRepoPath,username,password ;
	bool guestAccess = false ;
	LocalConfigurator lc ;
	m_progresslogfile = lc.getInstallPath() ;
	m_progresslogfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	m_progresslogfile += "bxadmin_progress.log" ;
	if( request.m_RequestPgs.m_Params.find( "NewRepositoryDeviceName" ) !=
		request.m_RequestPgs.m_Params.end() )
	{
		newRepoPath = request.m_RequestPgs.m_Params.find( "NewRepositoryDeviceName" )->second.value ;
	}
	if( isRepositoryConfiguredOnCIFS( newRepoPath )  )
	{
		if ( newRepoPath[newRepoPath.length() - 1 ] == '\\' )
			newRepoPath.erase( newRepoPath.length() - 1 ) ;
		
		if( request.m_RequestPgs.m_Params.find( "GuestAccess" ) != 
			request.m_RequestPgs.m_Params.end() )
		{
			if( request.m_RequestPgs.m_Params.find( "GuestAccess" )->second.value == "Yes" )
			{
				guestAccess = true ;
			}
		}
		
	}
	
	
	std::string errMsg ;
	errCode = MoveRepository(newRepoPath, errMsg,guestAccess ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

bool RepositoryHandler::CopyRetentionData(const std::string& srcVolName, 
										  const std::string& existingRepo,
										  const std::string& newRepo )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = true ;
	std::stringstream stream ;
	std::string retentionDataPath, newRetentionDataPath ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	protectionPairConfigPtr->GetRetentionPathBySrcDevName( srcVolName, retentionDataPath ) ;
	newRetentionDataPath = newRepo ;
	convertPath (existingRepo, retentionDataPath , newRepo ,newRetentionDataPath);
	std::string retentionDir, newRetentionDir ;
	retentionDir = retentionDataPath.substr( 0, retentionDataPath.find_last_of( ACE_DIRECTORY_SEPARATOR_CHAR ) ) ;
	newRetentionDir = newRetentionDataPath.substr( 0, newRetentionDataPath.find_last_of( ACE_DIRECTORY_SEPARATOR_CHAR )) ;
	if( boost::filesystem::exists(retentionDir.c_str()) )
	{
		if( !CopyDirectoryContents( retentionDir, newRetentionDir ) )
		{
			stream <<"Copy of directory contents from " << retentionDir << " to " << newRetentionDir << "failed" << std::endl; 
			ReportProgress(stream) ;
			DebugPrintf(SV_LOG_ERROR, "Failed to copy directory %s to %s\n",
				retentionDir.c_str(), newRetentionDir.c_str()) ;
			bRet = false ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool RepositoryHandler::CopyDirectoryContents(const std::string& src, const std::string& dest,bool recursive)
{
	bool rv = true;
	std::string filePattern = "cdp*";
	std::stringstream stream ;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	ACE_stat statBuff = {0};
	do
	{
		if( (sv_stat(getLongPathName(src.c_str()).c_str(), &statBuff) != 0) )
		{
			DebugPrintf(SV_LOG_ERROR, "CopyDirectoryContents Failed to stat file %s, error no: %d\n",
				src.c_str(),ACE_OS::last_error());
			rv = false;
			break;
		}
		if( (statBuff.st_mode & S_IFDIR) != S_IFDIR )
		{           
			DebugPrintf(SV_LOG_ERROR, "CopyDirectoryContents given file %s is not a directory\n",
				src.c_str());
			rv = false;
			break;
		}
		if(SVMakeSureDirectoryPathExists(dest.c_str()).failed())
		{
			DebugPrintf(SV_LOG_ERROR, "%s unable to create %s, error no: %d\n",
				FUNCTION_NAME, dest.c_str(),ACE_OS::last_error());
			rv = false;
			break;
		}
#ifdef SV_WINDOWS
		LocalConfigurator localconfigurator;
		if(localconfigurator.CDPCompressionEnabled())
		{
			if(!EnableCompressonOnDirectory(dest))
			{
				stringstream l_stderr;
				l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "Error during enabling compression on CDP Log Directory :" 
					<< dest << " .\n";
				//<< "Error Message: " << Error::Msg() << "\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
			}
		}
#endif

		ACE_DIR *pSrcDir = sv_opendir(src.c_str());
		if(pSrcDir == NULL)
		{
			DebugPrintf(SV_LOG_ERROR, "%s unable to open directory %s, error no: %d\n",
				FUNCTION_NAME, src.c_str(),ACE_OS::last_error());
			rv = false;
			break;
		}

		struct ACE_DIRENT *dEnt = NULL;
		SV_ULONGLONG filecount, total_size_on_disk, logicalsize, copied_size_on_disk ;
		copied_size_on_disk = 0 ;
		bool firstfile = true ;
		ACE_Time_Value timestart = ACE_OS::gettimeofday() ; 
		sv_get_filecount_spaceusage (src.c_str(), filePattern.c_str() ,filecount, total_size_on_disk, logicalsize) ;
		stream<<"\tCopying the " << convertToHumanReadableFormat(total_size_on_disk) << " of retention data" << std::endl ;
		SV_ULONGLONG lasttimelapsed = 99 ;
		while ((dEnt = ACE_OS::readdir(pSrcDir)) != NULL)
		{
			std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
			if ( dName == "." || dName == ".." )
				continue;

			std::string srcPath = src + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
			std::string destPath = dest + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
			ACE_stat statbuf = {0};
			if( (sv_stat(getLongPathName(srcPath.c_str()).c_str(), &statbuf) != 0))
			{
				DebugPrintf(SV_LOG_ERROR, "CDPUtil::CopyDirectoryContents given file %s does not exist.\n",
					srcPath.c_str());
				continue;
			}

			if((statbuf.st_mode & S_IFDIR) == S_IFDIR )
			{
				if(!recursive)
					continue;
				if(!CopyDirectoryContents(srcPath, destPath))
				{
					rv = false;
					break;
				}
			}
			else if((statbuf.st_mode & S_IFREG) == S_IFREG)
			{
				//reducing the copy time by checking the the file existance in new location and comparing the size of the file
				ACE_stat NewFileStat ={0};
				if((sv_stat(getLongPathName(destPath.c_str()).c_str(), &NewFileStat) == 0) && 
					(NewFileStat.st_size == statBuff.st_size))
				{
					continue;
				}
				if(RegExpMatch(filePattern.c_str(), dName.c_str()))
				{
					unsigned long err ;
					//boost::filesystem::copy_file(srcPath.c_str(),destPath.c_str();

					int retry = 0 ;
					do
					{
						retry++ ;
						if(!CopyFile(srcPath,destPath, err))
						{
							if( retry > 3 )
							{
								stream << "Failed to copy file "<< srcPath.c_str() << " to " << destPath.c_str() << std::endl ;
#ifdef SV_WINDOWS
								stream << "Error : " << GetErrorMessage(err) ;
#endif
								ReportProgress(stream) ;
								rv = false;
								break;
							}
						}
						else
						{
							copied_size_on_disk += File::GetSizeOnDisk(srcPath) ;
							if( copied_size_on_disk != 0 )
							{
								SV_ULONGLONG percentagecompleted, remainaingtime, timelapsed  ;
								getprogress(timestart, total_size_on_disk, copied_size_on_disk, percentagecompleted,
									timelapsed, remainaingtime) ;
								if( timelapsed != lasttimelapsed )
								{
									if( File::GetSizeOnDisk(srcPath) )
									{
										if( !firstfile )
										{
											std::cout<< "\r\t Copied " << convertToHumanReadableFormat(copied_size_on_disk) << "in " << secondsToHrf(timelapsed) ; 
										}
										else
										{
											std::cout<< "\n\t Copied " << convertToHumanReadableFormat (copied_size_on_disk )<< "in " << secondsToHrf(timelapsed) ;
										}
										if( firstfile )
										{
											firstfile = false ;
										}
										if( ( ( total_size_on_disk - copied_size_on_disk ) > 0 ) && remainaingtime != 0 )
										{
											std::cout<< "and estimated time left " << secondsToHrf(remainaingtime) ;
										}
										std::cout.flush() ;
									}
								}
								else
								{
									lasttimelapsed = timelapsed ;
								}
							}
							break ;
						}
					} while( true ) ;
				}
			}
		}
		if( dEnt == NULL )
		{
			std::cout<< std::endl; 
			std::cout.flush() ;
		}
		(void)ACE_OS::closedir(pSrcDir);

	} while(false);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool RepositoryHandler::get_filenames_withpattern(const std::string & dirname,const std::string& pattern,std::vector<std::string>& filelist )
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	ACE_stat statBuff = {0};
	do
	{
		if( (sv_stat(getLongPathName(dirname.c_str()).c_str(), &statBuff) != 0) )
		{
			DebugPrintf(SV_LOG_ERROR, "  Failed to stat file %s, error no: %d @LINE %d in FILE %s\n",
				dirname.c_str(),ACE_OS::last_error(),LINE_NO,FILE_NAME);
			rv = false;
			break;
		}
		if( (statBuff.st_mode & S_IFDIR) != S_IFDIR )
		{           
			DebugPrintf(SV_LOG_ERROR, "Given file %s is not a directory,@LINE %d in FILE %s\n",
				dirname.c_str(),LINE_NO,FILE_NAME);
			rv = false;
			break;
		}
		ACE_DIR *pSrcDir = sv_opendir(dirname.c_str());
		if(pSrcDir == NULL)
		{
			DebugPrintf(SV_LOG_ERROR, "%s unable to open directory %s, error no: %d @LINE %d in FILE %s\n",
				FUNCTION_NAME, dirname.c_str(),ACE_OS::last_error(),LINE_NO,FILE_NAME);
			rv = false;
			break;
		}

		struct ACE_DIRENT *dEnt = NULL;
		while ((dEnt = ACE_OS::readdir(pSrcDir)) != NULL)
		{
			std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
			if ( dName == "." || dName == ".." )
				continue;

			std::string srcPath = dirname + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
			ACE_stat statbuf = {0};
			if( (sv_stat(getLongPathName(srcPath.c_str()).c_str(), &statbuf) != 0))
			{
				DebugPrintf(SV_LOG_ERROR, "Given file %s does not exist.@LINE %d in FILE %s\n", srcPath.c_str(),LINE_NO,FILE_NAME);
				rv = false;
				break;
			}
			if((statbuf.st_mode & S_IFREG) == S_IFREG)
			{
				if(RegExpMatch(pattern.c_str(), dName.c_str()))
				{
					filelist.push_back(dName);
				}
			}
		}
		ACE_OS::closedir(pSrcDir);
	} while(false);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool RepositoryHandler::CleanAllDirectoryContent(const std::string & dbpath)
{
	bool rv = true;
	do
	{
		ACE_stat statBuff;
		std::stringstream stream;
		if( (sv_stat(getLongPathName(dbpath.c_str()).c_str(), &statBuff) != 0) )
		{
			stream << "Failed to stat "  << dbpath.c_str()  << " error no: " <<  ACE_OS::last_error() << std::endl;
			ReportProgress(stream) ;
			break;
		}
		if( (statBuff.st_mode & S_IFDIR) == S_IFDIR )
		{
			ACE_DIR *pSrcDir = sv_opendir(dbpath.c_str());
			if(pSrcDir == NULL)
			{
				stream << "Failed to open directory " << dbpath.c_str() << "error no: " << ACE_OS::last_error() << std::endl;
				ReportProgress(stream) ;
				break;
			}
			struct ACE_DIRENT *dEnt = NULL;
			while ((dEnt = ACE_OS::readdir(pSrcDir)) != NULL)
			{
				std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
				std::string filename = dbpath;
				filename += ACE_DIRECTORY_SEPARATOR_CHAR_A;
				filename += dName;
				if ( dName == "." || dName == ".." )
				{
					continue;
				}
				if(!CleanAllDirectoryContent(filename))	
				{
					rv = false;
				}			
			}
			ACE_OS::closedir(pSrcDir);
			if(ACE_OS::rmdir(getLongPathName(dbpath.c_str()).c_str()) < 0)
			{
				stream << "Failed to remove directory " <<dbpath.c_str()<< "error no: " << ACE_OS::last_error() << std::endl;
				ReportProgress(stream) ;
			}
		}
		else if (ACE_OS::unlink(getLongPathName(dbpath.c_str()).c_str()) != 0)
		{
			//stream << "unlink failed, path = " << dbpath.c_str() <<  " errno =  " << ACE_OS::last_error() << std::cout; 
			//rv = false;
		}

	}while(false);
	return rv;
}


				

bool RepositoryHandler::CopyVolpackFiles(const std::string& srcVolName, 
										 const std::string& existingRepo,
										 const std::string& newRepo,
										 const LocalConfigurator& lc,
										 bool compressedVolpacks, InmSparseAttributeState_t sparseattr_state)
{
	bool bRet = true ;
	std::stringstream stream ;
	std::string sparsefilename = existingRepo ;
	if( sparsefilename.length() == 1 )
	{
		sparsefilename += ":" ;
	}
	sparsefilename += "\\" ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance();
	std::string targetvolume ;
	protectionPairConfigPtr->GetTargetVolumeBySrcVolumeName( srcVolName,targetvolume ) ;
	sparsefilename = sparsefilename + lc.getHostId() + targetvolume.substr(targetvolume.find_last_of('\\')) ;
	boost::algorithm::replace_all( sparsefilename, "virtualvolume", "sparsefile") ; 
	DebugPrintf(SV_LOG_DEBUG, "sparsefile name %s\n", sparsefilename.c_str()) ;

	std::string volpackDirname, pattern ;
	pattern = sparsefilename.substr( sparsefilename.find_last_of("\\") + 1 ) ;
	pattern += "*" ;
	volpackDirname = sparsefilename.substr( 0, sparsefilename.find_last_of( "\\" ) );
	std::vector<std::string> filelist ;
	//CDPUtil::get_filenames_withpattern(volpackDirname, pattern, filelist) ;
	get_filenames_withpattern(volpackDirname, pattern, filelist) ;
	SV_ULONGLONG filecount , total_size_on_disk, logicalsize;
	sv_get_filecount_spaceusage (volpackDirname.c_str(), pattern.c_str() ,filecount,total_size_on_disk,logicalsize) ;
	SV_ULONGLONG copied_size_on_disk = 0 ;
	ACE_Time_Value timestart = ACE_OS::gettimeofday() ;
	std::vector<std::string>::const_iterator fileIter = filelist.begin() ;
	if( filelist.size() )
	{
		stream<<"\tCopying the " << convertToHumanReadableFormat(total_size_on_disk) << "of target volpack data"<<std::endl ;
		ReportProgress(stream) ;
	}
	SV_ULONGLONG lasttimelapsed = 99 ;
	bool firstfile = true ;
	while( filelist.end() != fileIter )
	{
		std::string srcFile, tgtFile ;
		srcFile = volpackDirname ;
		srcFile += ACE_DIRECTORY_SEPARATOR_CHAR ;
		srcFile += *fileIter ;
		tgtFile = newRepo ;
		convertPath (existingRepo, volpackDirname , newRepo ,tgtFile);
		SVMakeSureDirectoryPathExists( tgtFile.c_str() ) ;
		tgtFile += ACE_DIRECTORY_SEPARATOR_CHAR ;
		tgtFile += *fileIter ;
#ifdef SV_WINDOWS        
		unsigned long err ;
		if( CopySparseFile(srcFile, tgtFile, err) )
		{
			
			copied_size_on_disk +=  File::GetSizeOnDisk(srcFile) ;
			SV_ULONGLONG percentagecompleted, remainaingtime, timelapsed;
			if( copied_size_on_disk != 0 )
			{

				getprogress(timestart, total_size_on_disk, copied_size_on_disk, percentagecompleted,
							timelapsed, remainaingtime) ;
				if( lasttimelapsed != timelapsed )
				{
					if( File::GetSizeOnDisk(srcFile) )
					{
						if( !firstfile )
						{
							std::cout<< "\r\t Copied " << convertToHumanReadableFormat(copied_size_on_disk) << "in " << secondsToHrf(timelapsed) ;
						}
						else
						{
							std::cout<< "\n\t Copied " << convertToHumanReadableFormat(copied_size_on_disk) << "in " << secondsToHrf(timelapsed) ;
						}
						if( firstfile )
						{
							firstfile = false ;
						}
						if ( (total_size_on_disk - copied_size_on_disk ) > 0 && remainaingtime != 0 )
						{
							std::cout << "and estimated time left " << secondsToHrf(remainaingtime) ;
						}
						std::cout.flush() ;
					}
				}
				else
				{
					lasttimelapsed = timelapsed ;
				}
			}
			if( boost::filesystem::exists( tgtFile.c_str() ) )
			{
				if(sparseattr_state != E_INM_ATTR_STATE_DISABLED)
				{
					if(!SetSparseAttribute( tgtFile ) )
					{
						if(sparseattr_state == E_INM_ATTR_STATE_ENABLED)
						{
							bRet = false;
							stream<<"\n\tFailed to set the sparse attribute on " << *fileIter << std::endl ;
							ReportProgress(stream) ;
							break ;
						}
					}
				}

				if( compressedVolpacks )
				{
					if( !EnableCompressonOnFile( tgtFile ) )
					{
						bRet = false;
						stream<<"n\tFailed to set the compression on " << *fileIter << std::endl ;
						ReportProgress(stream) ;
						break ;
					}
				}
			}
		}
		else
		{
			stream<< "\n\tFailed to copy the volpack file " << *fileIter << std::endl ;
#ifdef SV_WINDOWS
			stream<< "\n\tError : " << GetErrorMessage(err) ;
#endif

			ReportProgress(stream) ;
			DebugPrintf(SV_LOG_ERROR, "Failed to copy %s to %s\n", 
				srcFile.c_str(), tgtFile.c_str()) ;
			bRet = false;
			break ;
		}
#endif
		fileIter++ ;
	}
	if( fileIter == filelist.end() )
	{
		std::cout<< std::endl ;
		std::cout.flush(); 
	}
	return bRet ;
}

bool RepositoryHandler::CopyConfigurationFiles( const std::string& existingRepo,
											   const std::string& newRepo) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = true ;
	std::vector<std::string> configFiles ;
	ConfSchemaMgr::GetInstance()->GetConfigurationFiles( configFiles ) ;
	std::stringstream stream ;
	std::vector<std::string>::const_iterator confFileIter = configFiles.begin() ;
	while( confFileIter != configFiles.end() )
	{
		std::string tgtConfFile = newRepo ;
		convertPath (existingRepo, *confFileIter , newRepo ,tgtConfFile);
		SVMakeSureDirectoryPathExists( tgtConfFile.substr( 0, tgtConfFile.find_last_of( '\\' ) ).c_str() )  ;
		std::string relFilePath ;
		relFilePath = tgtConfFile.substr(tgtConfFile.find_last_not_of('\\')) ;
		unsigned long err ;
		if( !CopyFile( *confFileIter, tgtConfFile, err ) )
		{
			stream << "Failed to copy file "<< confFileIter->c_str() << " to " << tgtConfFile.c_str() << std::endl ;
#ifdef SV_WINDOWS
			stream << "Error : " << GetErrorMessage(err) ;
#endif
			ReportProgress(stream) ;
			DebugPrintf(SV_LOG_ERROR, "Failed to copy %s to %s\n", 
				confFileIter->c_str(),
				tgtConfFile.c_str()) ;
			bRet = false ;
			break ;
		}
		confFileIter++ ;
	}
	if( bRet )
	{
		LocalConfigurator configurator ;
		std::string existingConfigDat, newConfigDat ;
		existingConfigDat = existingRepo ;
		newConfigDat = newRepo ;
		if( existingRepo.length() == 1 )
		{
			existingConfigDat += ":" ;
		}
		if( newRepo.length() == 1 )
		{
			newConfigDat += ":" ;
		}
		existingConfigDat += ACE_DIRECTORY_SEPARATOR_CHAR ;
		existingConfigDat += configurator.getHostId() ;
		existingConfigDat += ACE_DIRECTORY_SEPARATOR_CHAR ;
		existingConfigDat += "config.dat" ;

		newConfigDat += ACE_DIRECTORY_SEPARATOR_CHAR ;
		newConfigDat += configurator.getHostId() ;;
		newConfigDat += ACE_DIRECTORY_SEPARATOR_CHAR ;
		newConfigDat += "config.dat.1" ;
		if( boost::filesystem::exists(existingConfigDat.c_str()) )
		{
			unsigned long err ;
			if( !CopyFile( existingConfigDat, newConfigDat, err ) )
			{
				stream << "Failed to copy file "<< existingConfigDat << " to " << newConfigDat.c_str() << std::endl ;
#ifdef SV_WINDOWS
				stream << "Error : " << GetErrorMessage(err) ;
#endif
				ReportProgress(stream) ;
				bRet = false ;
			}
			else
			{
				stream << "Successfully copied config.dat" <<std::endl ;
				ReportProgress(stream) ;
				ACE_OS::unlink(existingConfigDat.c_str()) ;
			}
		}
#ifdef SV_WINDOWS
		std::ofstream of( existingConfigDat.c_str() ) ;
		if( of.is_open() )
		{
			InitialSettings settings ;
			of << CxArgObj<InitialSettings>( settings ) ;
			of.flush(); 
			of.close() ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "unable to create the dummy settings file\n"); 
		}
#endif
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool RepositoryHandler::CopyRepositoryContents( const std::string& existingRepo, 
											   const std::string& newRepo )
{
	bool bRet = true ;
	ProtectionPairConfigPtr protectionPairConfigPtr ;
	protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	std::list<std::string> protectedVolumes ;
	std::stringstream stream ;
	protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
	LocalConfigurator configurator ;
	bool compressedVolpacks = configurator.VirtualVolumeCompressionEnabled() ;

	std::list<std::string>::const_iterator protectedVolIter = protectedVolumes.begin() ;
	do
	{
		InmSparseAttributeState_t  sparseattr_state = E_INM_ATTR_STATE_ENABLEIFAVAILABLE;

		SV_UINT sparseattr_configvalue = configurator.getVolpackSparseAttribute();;
		if(sparseattr_configvalue == E_INM_ATTR_STATE_ENABLED ||
			sparseattr_configvalue == E_INM_ATTR_STATE_DISABLED ||
			sparseattr_configvalue == E_INM_ATTR_STATE_ENABLEIFAVAILABLE)
		{
			sparseattr_state = static_cast<InmSparseAttributeState_t> (sparseattr_configvalue);
		} else
		{
			DebugPrintf(SV_LOG_ERROR, 
				"Invalid value for config parameter VolpackSparseAttribute in Vx configuration file.\n");
			bRet = false;
			break;
		}

		while( protectedVolIter != protectedVolumes.end() )
		{
			stream<<"\n\nStarting copying the data for " << protectedVolIter->c_str() << std::endl ;
			ReportProgress(stream) ;
			stream<<"\n\tCopying the retention data."<< std::endl ;
			ReportProgress(stream) ;
			if( !CopyRetentionData( *protectedVolIter, existingRepo, newRepo ) )
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to copy the retention data\n") ;
				stream<<"\tFailed to copy the retention data "<< std::endl ;
				ReportProgress(stream) ;
				bRet = false ;
				break ;
			}
			stream<<"\tSuccessfully copied retention data " << std::endl ;
			ReportProgress(stream) ;
			if( !CopyVolpackFiles( *protectedVolIter, existingRepo, newRepo, configurator, compressedVolpacks, sparseattr_state ) )
			{
				stream<<"\tFailed to copy the target volpack data " << std::endl ;
				ReportProgress(stream) ;
				DebugPrintf(SV_LOG_ERROR, "Failed to copy the volpack files\n") ;
				bRet = false ;
				break ;
			}
			protectedVolIter++ ;
		}
		if( bRet == false )
		{
			break ;
		}
		if(!CopyMbrDirectory( existingRepo, newRepo ) )
		{
			stream<<"\tFailed to copy the Mbr files to new backup location device "<< newRepo << std::endl ;
			ReportProgress(stream) ;
			bRet = false ;
		}
		else
		{
			stream<<"\tSuccessfully copied the Mbr files to new backup location device "<< newRepo << std::endl ;
			ReportProgress(stream) ;
		}
		if(!CopyLogsDirectory( existingRepo, newRepo ) )
		{
			stream<<"\tFailed to copy the Logs files to new backup location device "<< newRepo << std::endl ;
			ReportProgress(stream) ;
			bRet = false ;
		}
		else
		{
			stream<<"\tSuccessfully copied the Logs files to new backup location device "<< newRepo << std::endl ;
			ReportProgress(stream) ;
		}

	} while( 0 ) ;
	return bRet ;
}

bool RepositoryHandler::CopyLogsDirectory( const std::string& existingRepo, 
										  const std::string& newRepo )
{
	std::string existinglogsDir, newLogsDir ;
	bool bRet = true ;
	existinglogsDir = existingRepo ;
	newLogsDir = newRepo ;
	//std::stringstream ;
	LocalConfigurator configurator ;
	if( existingRepo.length() == 1 )
	{
		existinglogsDir += ":" ;
	}
	if( newRepo.length() == 1 )
	{
		newLogsDir += ":" ;
	}
	existinglogsDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	newLogsDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	existinglogsDir += configurator.getHostId();
	newLogsDir += configurator.getHostId();
	existinglogsDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	newLogsDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	existinglogsDir += "Logs";
	newLogsDir += "Logs";
	std::stringstream stream ;
	bool logsDirectoryExists = boost::filesystem::is_directory (existinglogsDir.c_str());
	if (logsDirectoryExists)
	{
		std::vector<std::string> filelist ;
		std::string pattern = "*" ;
		get_filenames_withpattern(existinglogsDir, pattern, filelist) ;
		std::vector<std::string>::const_iterator fileIter = filelist.begin() ;
		boost::filesystem::create_directory (newLogsDir.c_str());
		SVMakeSureDirectoryPathExists( newLogsDir.c_str() ) ;
		while( filelist.end() != fileIter )
		{
			std::string existingLogsFilePath = existinglogsDir ;
			existingLogsFilePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
			existingLogsFilePath += *fileIter ;
			std::string newLogsFilePath = newLogsDir ;
			newLogsFilePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
			newLogsFilePath += *fileIter ;
			unsigned long err ;
			if( !CopyFile( existingLogsFilePath, newLogsFilePath, err ) )
			{
				bRet = false ;
				stream << "Failed to copy file "<< existingLogsFilePath << " to " << newLogsFilePath.c_str() << std::endl ;
#ifdef SV_WINDOWS
				stream << "Error : " << GetErrorMessage(err) ;
#endif
				ReportProgress(stream) ;
			}
			fileIter++ ;
		}
		stream<<"Copied " << filelist.size() << " Logs files to new backup location device. " << std::endl ;
		ReportProgress(stream) ;
	}
	return bRet ;
}
bool RepositoryHandler::CopyMbrDirectory( const std::string& existingRepo, 
										 const std::string& newRepo )
{
	std::string existingMbrDir, newMbrDir ;
	bool bRet = true ;
	existingMbrDir = existingRepo ;
	newMbrDir = newRepo ;
	std::stringstream stream ;
	LocalConfigurator configurator ;
	if( existingRepo.length() == 1 )
	{
		existingMbrDir += ":" ;
	}
	if( newRepo.length() == 1 )
	{
		newMbrDir += ":" ;
	}
	existingMbrDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	newMbrDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	existingMbrDir += "SourceMBR" ;
	newMbrDir += "SourceMBR" ;
	existingMbrDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	newMbrDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
	existingMbrDir += configurator.getHostId() ;
	newMbrDir += configurator.getHostId() ;
	std::vector<std::string> filelist ;
	std::string pattern = "mbrdata_*" ;
	//CDPUtil::get_filenames_withpattern(existingMbrDir, pattern, filelist) ;
	get_filenames_withpattern(existingMbrDir, pattern, filelist) ;
	std::vector<std::string>::const_iterator fileIter = filelist.begin() ;
	SVMakeSureDirectoryPathExists( newMbrDir.c_str() ) ;
	while( filelist.end() != fileIter )
	{
		std::string existingMbrFilePath = existingMbrDir ;
		existingMbrFilePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
		existingMbrFilePath += *fileIter ;
		std::string newMbrFilePath = newMbrDir ;
		newMbrFilePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
		newMbrFilePath += *fileIter ;
		unsigned long err ;
		if( !CopyFile( existingMbrFilePath, newMbrFilePath, err ) )
		{
			bRet = false ;
			stream << "Failed to copy file "<< existingMbrFilePath << " to " << newMbrFilePath.c_str() << std::endl ;
#ifdef SV_WINDOWS
			stream << "Error : " << GetErrorMessage(err) ;
#endif
			ReportProgress(stream) ;
		}
		fileIter++ ;
	}
	stream<<"Copied " << filelist.size() << " Mbr files to new backup location device. " << std::endl ;
	ReportProgress(stream) ;
	return bRet ;
}

INM_ERROR RepositoryHandler::DeleteRepository(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	bool proceed = true ;
	bool bShouldDeleteProtection = false ;
	ConstParametersIter_t paramIter ;
	paramIter = request.m_RequestPgs.m_Params.find( "DeleteProtection") ;
	if( paramIter != request.m_RequestPgs.m_Params.end() )
	{
		if( paramIter->second.value.compare("Yes") == 0 )
		{
			bShouldDeleteProtection = true ;
		}
	}
	if( !bShouldDeleteProtection ) 
	{
		errCode = E_PROTECTIONS_FOUND ;
		proceed = false ;
	}
	if( proceed )
	{
		LocalConfigurator configurator ;
		std::string agentGuid = configurator.getHostId() ;
		std::string repoLocationForHost = constructConfigStoreLocation( ConfSchemaMgr::GetInstance()->getRepoLocation(), agentGuid ) ;
		if( repoLocationForHost[repoLocationForHost.length() - 1 ] ==
			ACE_DIRECTORY_SEPARATOR_CHAR )
		{
			repoLocationForHost.erase( repoLocationForHost.length() - 1 ) ;
		}
		bool checkRepoAvailability = true ;
		FunctionInfo requestForRepo = request ;
		if ( ListConfiguredRepositories(requestForRepo) == E_REPO_NOT_AVAILABLE )
		{
			checkRepoAvailability = false ;
		}
		DeleteRepositoryContents(bShouldDeleteProtection) ;
		RemCredentialsFromRegistry() ;
		if ( !checkRepoAvailability )
		{
			errCode = E_REPO_NOT_AVAILABLE ;
			request.m_Message += "Stop the backupagent service, delete the contents manually from " + repoLocationForHost + " folder before setting up the backup location. " ;
			request.m_Message += "Start the backupagent service. " ;
		
		}
		else
		{
			errCode = E_SUCCESS; 
		}
	}
	StartSvc("backupagent") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

void RepositoryHandler::RemoveAgentConfigData()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator configurator ;
	char regName[26];
	int counter = configurator.getVirtualVolumesId();
	while(counter!=0)
	{
		inm_sprintf_s(regName, ARRAYSIZE(regName), "VirVolume%d", counter);
		configurator.setVirtualVolumesPath(regName,"");
		counter--;
	}
	std::string appagentCachePath, schedulerCachePath ;
	appagentCachePath = configurator.getCacheDirectory() + "\\" + "etc" + "\\" + "AppAgentCache.dat";
	schedulerCachePath = configurator.getCacheDirectory() + "\\" + "etc" + "\\" + "SchedulerCache.dat" ;
	std::string appagentCacheLoclPath = appagentCachePath + ".lck";
	std::string diskinfocachepath = configurator.getCacheDirectory() + "\\" + "etc" + "\\" + "diskslabelchecksum.dat" ;
	std::string diskinfocachelckpath = diskinfocachepath + ".lck" ;
	if( boost::filesystem::exists( diskinfocachepath.c_str() ) )
	{
		ACE_File_Lock lock(getLongPathName(diskinfocachelckpath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
		lock.acquire_read();
		try
		{
			boost::filesystem::remove( diskinfocachepath.c_str() ) ;
		}
		catch( std::exception& e)
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to unlink the files %s\n", e.what()) ;
		}
		lock.release() ;
	}
	if( boost::filesystem::exists( appagentCachePath.c_str() ) )
	{
		ACE_File_Lock lock(getLongPathName(appagentCacheLoclPath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
		lock.acquire_read();
		try
		{
			boost::filesystem::remove( appagentCachePath.c_str() ) ;
		}
		catch( std::exception& e)
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to unlink the files %s\n", e.what()) ;
		}
		lock.release() ;
	}
	if( boost::filesystem::exists( schedulerCachePath.c_str() ) )
	{
		try
		{
			boost::filesystem::remove( schedulerCachePath.c_str() ) ;
		}
		catch( std::exception& e)
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to unlink the files %s\n", e.what()) ;
		}
	}
	configurator.unsetRepositoryLocation() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void RepositoryHandler::DeleteRepositoryContents(bool bShouldDeleteProtection)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator configurator ;
	std::string agentGuid = configurator.getHostId() ;
	std::string repoLocationForHost = constructConfigStoreLocation( ConfSchemaMgr::GetInstance()->getRepoLocation(), agentGuid ) ;

	if( bShouldDeleteProtection )
	{
		if( repoLocationForHost[repoLocationForHost.length() - 1 ] ==
			ACE_DIRECTORY_SEPARATOR_CHAR )
		{
			repoLocationForHost.erase( repoLocationForHost.length() - 1 ) ;
		}
		try
		{
			DeleteOldRepositoryContents(repoLocationForHost, true) ;
		}
		catch(...)
		{
			RemoveAgentConfigData() ;
			throw TransactionException("Unable to find availability of " + 
				repoLocationForHost + 
				" or its sub directories", 
				E_REPO_NOT_AVAILABLE) ;
		}
	}
	else
	{
		ConfSchemaReaderWriterPtr readerWriterPtr = ConfSchemaReaderWriter::GetInstance() ;

		VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
		volumeConfigPtr->DeleteRepositoryObject();
		AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
		AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
		alertConfigPtr->RemoveAlerts();
		auditConfigPtr->RemoveAuditMessages();
		readerWriterPtr->Sync() ;
	}
	RemoveAgentConfigData() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


INM_ERROR RepositoryHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}


void RepositoryHandler::GetEligibleRepoDevices(std::map<std::string,RepoDeviceDetails>& repoDevDetails)
{
	VolumeSummaries_t volumeSummaries;
	VolumeDynamicInfos_t volumeDynamicInfos;
	VolumeInfoCollector volumeCollector;
	volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false); 
	VolumeSummaries_t::const_iterator volSummaryIterB = volumeSummaries.begin() ;
	VolumeSummaries_t::const_iterator volSummaryIterE = volumeSummaries.end() ;
	while( volSummaryIterB != volSummaryIterE )
	{
		if( volSummaryIterB->capacity != 0 && 
			!volSummaryIterB->isSystemVolume && 
			volSummaryIterB->volumeLabel != "System Reserved" && 
			( volSummaryIterB->fileSystem.compare("NTFS") == 0 || 
			  volSummaryIterB->fileSystem.compare("ReFS") == 0 || 
			  volSummaryIterB->fileSystem.compare("ExtendedFAT") == 0 ) &&
			( volSummaryIterB->type == VolumeSummary::DISK ||
			volSummaryIterB->type == VolumeSummary::PARTITION || 
			volSummaryIterB->type == VolumeSummary::LOGICAL_VOLUME || 
			volSummaryIterB->type == VolumeSummary::DISK_PARTITION ) )
		{
			ConstVolumeDynamicInfosIter volDynamicInfoIterB = volumeDynamicInfos.begin() ;
			ConstVolumeDynamicInfosIter volDynamicInfoIterE = volumeDynamicInfos.end() ;
			while( volDynamicInfoIterB != volDynamicInfoIterE )
			{
				if( volDynamicInfoIterB->name == volSummaryIterB->name )
				{
					break ;
				}
				volDynamicInfoIterB++ ;
			}
			if( volDynamicInfoIterB != volDynamicInfoIterE  ) 
			{
				std::string VolumeMonuntPoint = volSummaryIterB->name;
				if(VolumeMonuntPoint.length() <= 3)
				{
					VolumeMonuntPoint = VolumeMonuntPoint[0];
				}
				else if(VolumeMonuntPoint.length() > 3 && 
					VolumeMonuntPoint[VolumeMonuntPoint.length() - 1] == '\\' )
				{
					VolumeMonuntPoint.erase(VolumeMonuntPoint.length() - 1) ;
				}
				RepoDeviceDetails repDevDetails ;
				repDevDetails.capacity = volSummaryIterB->capacity ;
				repDevDetails.freeSpace = volDynamicInfoIterB->freeSpace ;
				repDevDetails.writeCacheState = volSummaryIterB->writeCacheState;
				repoDevDetails.insert(std::make_pair( VolumeMonuntPoint,repDevDetails)) ; 
			}
		}
		volSummaryIterB++ ;
	}
}

void RepositoryHandler::GetEligibleVolumes(std::map<std::string,VolumeDetails>& eligibleVolumes)
{
	VolumeSummaries_t volumeSummaries;
	VolumeDynamicInfos_t volumeDynamicInfos;
	VolumeInfoCollector volumeCollector;
	volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false); 
	VolumeSummaries_t::const_iterator volSummaryIterB = volumeSummaries.begin() ;
	VolumeSummaries_t::const_iterator volSummaryIterE = volumeSummaries.end() ;
	while( volSummaryIterB != volSummaryIterE )
	{
		if( volSummaryIterB->capacity != 0 && 
			( volSummaryIterB->fileSystem.compare("NTFS") == 0 || 
			  volSummaryIterB->fileSystem.compare("ReFS") == 0 ||
			  volSummaryIterB->fileSystem.compare("ExtendedFAT") == 0 ) &&
			( volSummaryIterB->type == VolumeSummary::DISK ||
			volSummaryIterB->type == VolumeSummary::PARTITION || 
			volSummaryIterB->type == VolumeSummary::LOGICAL_VOLUME || 
			volSummaryIterB->type == VolumeSummary::DISK_PARTITION ) )
		{
			ConstVolumeDynamicInfosIter volDynamicInfoIterB = volumeDynamicInfos.begin() ;
			ConstVolumeDynamicInfosIter volDynamicInfoIterE = volumeDynamicInfos.end() ;
			while( volDynamicInfoIterB != volDynamicInfoIterE )
			{
				if( volDynamicInfoIterB->name == volSummaryIterB->name )
				{
					break ;
				}
				volDynamicInfoIterB++ ;
			}
			if( volDynamicInfoIterB != volDynamicInfoIterE  ) 
			{
				std::string VolumeMonuntPoint = volSummaryIterB->name;
				if(VolumeMonuntPoint.length() <= 3)
				{
					VolumeMonuntPoint = VolumeMonuntPoint[0];
				}
				else if(VolumeMonuntPoint.length() > 3 && 
					VolumeMonuntPoint[VolumeMonuntPoint.length() - 1] == '\\' )
				{
					VolumeMonuntPoint.erase(VolumeMonuntPoint.length() - 1) ;
				}
				VolumeDetails volumeDetails ;
				volumeDetails.capacity = volSummaryIterB->capacity ;
				volumeDetails.freeSpace = volDynamicInfoIterB->freeSpace ;
				volumeDetails.writeCacheState = volSummaryIterB->writeCacheState;
				volumeDetails.mountPoint = 	volSummaryIterB->mountPoint;
				volumeDetails.volumeLabel = volSummaryIterB->volumeLabel;
				volumeDetails.isSystemVolume = volSummaryIterB->isSystemVolume;
				eligibleVolumes.insert(std::make_pair( VolumeMonuntPoint,volumeDetails)) ; 
			}
		}
		volSummaryIterB++ ;
	}
}

INM_ERROR RepositoryHandler::ListRepositoryDevices(FunctionInfo& request)
{
#ifdef SV_WINDOWS
			mountBootableVolumesIfRequried() ;
#endif
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	bool isVolumeEligibleForRepository = false ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	std::string reponame,repopath,installedVolumeName;

	try 
	{
		LocalConfigurator configurator ;
		std::string installedDirectory = configurator.getInstallPath() ;
		size_t found;
		if (( found = installedDirectory.find (":\\") ) != std::string::npos )
		{
			installedVolumeName = installedDirectory.substr (0,found);
		}
		repopath = configurator.getRepositoryLocation() ;
	}
	catch(ContextualException&  ex)
	{
		DebugPrintf(SV_LOG_DEBUG, "exception while obtaining the repository name from drscout.conf %s\n", ex.what()) ;
	}

	if (repopath.empty())
	{  	
		std::map<std::string,RepoDeviceDetails> repoDevDetails ;
		GetEligibleRepoDevices(repoDevDetails)  ;
		std::map<std::string,RepoDeviceDetails>::const_iterator repoDevDetailIter ;
		repoDevDetailIter = repoDevDetails.begin() ;
		DebugPrintf (SV_LOG_DEBUG, "repoDevDetails Size is %d \n",repoDevDetails.size() ); 
		while( repoDevDetailIter != repoDevDetails.end())
		{
			if ( installedVolumeName != repoDevDetailIter->first) 
			{
				ParameterGroup deviceNamePg ;
				ValueType vt ;
				vt.value = repoDevDetailIter->first ;
				deviceNamePg.m_Params.insert(std::make_pair("DeviceName", vt)) ;
				vt.value = boost::lexical_cast<std::string>(repoDevDetailIter->second.capacity) ;
				deviceNamePg.m_Params.insert(std::make_pair("RepositoryDeviceCapacity", vt)) ;
				if (repoDevDetailIter->second.writeCacheState == 0)
					vt.value = "Unknown";
				else if(repoDevDetailIter->second.writeCacheState == 1)
					vt.value = "Disabled";
				else 
					vt.value = "Enabled";
				deviceNamePg.m_Params.insert(std::make_pair("DiskCacheStatus",vt));
				vt.value = boost::lexical_cast<std::string>(repoDevDetailIter->second.freeSpace) ;
				deviceNamePg.m_Params.insert(std::make_pair("RepositoryDeviceFreeSpace", vt)) ;
				vt.value = "Logicalvolume" ;
				deviceNamePg.m_Params.insert(std::make_pair("DeviceType", vt)) ;
				//Bug 20159 - For Disk Type
				vt.value = getBusType(repoDevDetailIter->first);
				deviceNamePg.m_Params.insert(std::make_pair("StorageBusType", vt)) ;
				request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(repoDevDetailIter->first, deviceNamePg)) ;                
				isVolumeEligibleForRepository = true;
			}
			repoDevDetailIter++ ;
		}
		if (isVolumeEligibleForRepository == false )
		{
			errCode = E_NO_REPO_DEVICE_FOUND;
		}
		else
		{	
			errCode = E_SUCCESS ;
		}		
	}
	else 
	{  
		volumeConfigPtr->GetRepositoryNameAndPath(reponame, repopath)  ;
		DebugPrintf( SV_LOG_DEBUG, "Backup location name: %s, Length: %d\n", reponame.c_str(), reponame.length() ) ;
		if( volumeConfigPtr->isRepositoryAvailable(reponame) )
		{
			errCode = E_REPO_ALREADY_CONFIGURED;
		}
		else
		{
			errCode = E_REPO_CREATION_INPROGRESS ;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

bool RepositoryHandler::IsDeviceAvailableinSystem(std::string& VolumeName, VolumeSummary& foundDevice )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string DeviceName = VolumeName;
	if(DeviceName.length() <= 3)
	{
		DeviceName = DeviceName[0];
	}
	else if(DeviceName.length() > 3 && 
		DeviceName[DeviceName.length() - 1] == '\\' )
	{
		DeviceName.erase(DeviceName.length() - 1) ;
	}

	bool bdeviceavailable = false;
	VolumeSummaries_t volumeSummaries;
	VolumeDynamicInfos_t volumeDynamicInfos;
	VolumeInfoCollector volumeCollector;
	volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false); // TODO: should have a configurator option to deterime
	VolumeSummaries_t::const_iterator volSummaryIterB = volumeSummaries.begin() ;
	VolumeSummaries_t::const_iterator volSummaryIterE = volumeSummaries.end() ;
	while( volSummaryIterB != volSummaryIterE )
	{
		if( volSummaryIterB->capacity != 0 && 
			!volSummaryIterB->isSystemVolume && 
			( volSummaryIterB->type == VolumeSummary::DISK ||
			volSummaryIterB->type == VolumeSummary::PARTITION || 
			volSummaryIterB->type == VolumeSummary::LOGICAL_VOLUME || 
			volSummaryIterB->type == VolumeSummary::DISK_PARTITION ) )
		{
			ConstVolumeDynamicInfosIter volDynamicInfoIterB = volumeDynamicInfos.begin() ;
			ConstVolumeDynamicInfosIter volDynamicInfoIterE = volumeDynamicInfos.end() ;
			while( volDynamicInfoIterB != volDynamicInfoIterE )
			{
				if( volDynamicInfoIterB->name == volSummaryIterB->name )
				{
					break ;
				}
				volDynamicInfoIterB++ ;
			}
			if( volDynamicInfoIterB != volDynamicInfoIterE)
			{
				std::string VolumeMonuntPoint = volSummaryIterB->name;
				if(VolumeMonuntPoint.length() <= 3)
				{
					VolumeMonuntPoint = VolumeMonuntPoint[0];
				}
				else if(VolumeMonuntPoint.length() > 3 && 
					VolumeMonuntPoint[VolumeMonuntPoint.length() - 1] == '\\' )
				{
					VolumeMonuntPoint.erase(VolumeMonuntPoint.length() - 1) ;
				}
				if(InmStrCmp<NoCaseCharCmp>(VolumeMonuntPoint,DeviceName) == 0)
				{
					bdeviceavailable = true;
					foundDevice = *volSummaryIterB ;
					break;
				}
			}
		}
		volSummaryIterB++ ;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bdeviceavailable;
}

void RepositoryHandler::UnmountAllVirtualVolumes()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator configurator ;

	char regName[26];
	int counter=configurator.getVirtualVolumesId();
	while(counter!=0)
	{
		inm_sprintf_s(regName, ARRAYSIZE(regName), "VirVolume%d", counter);

		std::string data = configurator.getVirtualVolumesPath(regName);
		std::string sparsefilename;
		std::string volume;

		if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
		{
			counter--;
			continue;
		}

		std::string new_value="";
		configurator.setVirtualVolumesPath(regName,new_value);
		counter--;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void RepositoryHandler::DoStopFilteringForAllVolumes()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::map<std::string,VolumeDetails> eligibleVolumes ;
	GetEligibleVolumes(eligibleVolumes) ;
	LocalConfigurator lc;
	std::map<std::string,VolumeDetails>::iterator eligiblevolIter = eligibleVolumes.begin() ;
	while( eligibleVolumes.end() != eligiblevolIter )
	{
		SV_ULONG dwExitCode = 0 ;
		std::string logfile ;
		std::string stdOutput ;
		bool bProcessActive = true;

		std::string stopfscmdstr = lc.getInstallPath() ;
		stopfscmdstr += ACE_DIRECTORY_SEPARATOR_CHAR ;
		stopfscmdstr += "drvutil.exe" ;
		stopfscmdstr +=  " --StopFiltering " ;
		stopfscmdstr +=  eligiblevolIter->first ;
		logfile = lc.getInstallPath() ;
		logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
		logfile += "stopFiltering.log" ;
		AppCommand stopfilteringcmd (stopfscmdstr, 0, logfile) ;
		stdOutput = "" ;
		stopfilteringcmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
		eligiblevolIter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
INM_ERROR RepositoryHandler::SetupRepository(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_NO_REPO_CONFIGERED ;
	LocalConfigurator configurator ;
	VolumeSummary foundDevice ;
	bool bFoundDevice = false ;
	bool compression = true ;
	bool isGuestAccess = false ;
	std::string username, password, domain ;
#ifdef SV_WINDOWS
	InmServiceStartType serviceType ; 
	std::string serviceStartUpType; 
	if (  getStartServiceType ("backupagent", serviceType , serviceStartUpType) == true ) 
	{
		if (serviceStartUpType != "Automatic")
		{
			if ( changeServiceType ("backupagent",INM_SVCTYPE_AUTO) !=  SVS_OK) 
			{
				DebugPrintf (SV_LOG_DEBUG, "Unable to Get Start Service Type %d", ACE_OS::last_error() ); 
			}
			else 
			{
				DebugPrintf (SV_LOG_DEBUG, "Change the Service Type successfully \n"); 
			}
		}
		StartSvc("backupagent") ;		
	}
	else 
	{
		DebugPrintf (SV_LOG_DEBUG, "Unable to Get Start Service Type %d \n", ACE_OS::last_error() );
	}
#endif 
	try
	{
		std::string LogPath = configurator.getRepositoryLocation();
		if( isRepositoryConfiguredOnCIFS( LogPath ) )
		{
			errCode = E_REPO_ALREADY_CONFIGURED ;
		}
		else if( ( bFoundDevice = IsDeviceAvailableinSystem(LogPath, foundDevice ) ) == true )
		{
			errCode = E_REPO_ALREADY_CONFIGURED ;
		}
		else
		{
			errCode = E_STORAGE_PATH_NOT_FOUND;
		}
	}
	catch(ContextualException& ex)
	{
		DebugPrintf(SV_LOG_DEBUG, "exception1 %s\n", ex.what()) ;
	}
	if( errCode == E_NO_REPO_CONFIGERED  )
	{
		DLM_ERROR_CODE dlmErrCode = DeleteDlmRegitryEntries() ;
		if( dlmErrCode != DLM_ERR_SUCCESS )
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to cleanup the dlm registry entries %d\n", dlmErrCode) ;
		}
		std::string repositoryName, repositoryDeviceName ;
		if( request.m_RequestPgs.m_Params.find("RepositoryName") != 
			request.m_RequestPgs.m_Params.end() )
		{
			repositoryName = request.m_RequestPgs.m_Params.find("RepositoryName")->second.value ;
		}
		if( request.m_RequestPgs.m_Params.find( "EnableCompression" ) != request.m_RequestPgs.m_Params.end() && 
			request.m_RequestPgs.m_Params.find( "EnableCompression" )->second.value != "Yes" )
		{
			compression = false ;
		}
		if( request.m_RequestPgs.m_Params.find("RepositoryDeviceName") != 
			request.m_RequestPgs.m_Params.end() )
		{
			repositoryDeviceName = request.m_RequestPgs.m_Params.find("RepositoryDeviceName")->second.value ;

			if(repositoryDeviceName.length() <= 3)
			{
				repositoryDeviceName = repositoryDeviceName[0];
			}
			else if(repositoryDeviceName.length() > 3 && 
				repositoryDeviceName[repositoryDeviceName.length() - 1] == '\\' )
			{
				repositoryDeviceName.erase(repositoryDeviceName.length() - 1) ;
			}
		}
		if( request.m_RequestPgs.m_Params.find("GuestAccess") != 
			request.m_RequestPgs.m_Params.end() )
		{
			if( request.m_RequestPgs.m_Params.find("GuestAccess")->second.value == "Yes" )
			{
				isGuestAccess = true ;
			}
			else
			{
				isGuestAccess = false ;
			}
		}
		else
		{
			isGuestAccess = configurator.isGuestAccess() ;
		}
		if( !isGuestAccess )
		{
			std::string msg ;
			GetCredentialsFromRequest(request, username, password, domain, msg) ;
		}
		if( isRepositoryConfiguredOnCIFS( repositoryDeviceName ) )
		{
			std::string errmsg;
			do
			{
				if( !isGuestAccess && username.empty() )
				{
					if(  CredentailsAvailable() )
					{
						GetCredentials(domain, username, password ) ;
					}
					else 
					{
						errCode = E_UNKNOWN_CREDENTIALS ;
						break ;
					}
				}
				if( AccessRemoteShare(repositoryDeviceName, username, password, domain, isGuestAccess, errCode,  errmsg, true) )
				{
					/*SaveCredentialsInReg(username, password, domain, isGuestAccess, errCode, errmsg ) ;*/
					persistAccessCredentials(repositoryDeviceName,username, password, domain,errCode,errmsg,isGuestAccess);
				}
			} while( false ) ;
			if( errCode != E_SUCCESS )
			{
				request.m_Message = errmsg ;
			}
		}
#ifdef SV_WINDOWS
		else
		{
			SV_UINT physicalSecSize ;
			errCode = E_SUCCESS ;
			cdp_volume_util u;
			if( u.GetPhysicalSectorSize( repositoryDeviceName, physicalSecSize) )
			{
				if( physicalSecSize != 512 )
				{
					errCode = E_REPO_UNSUPPORTED_SECTORSIZE ;
					request.m_Message = "Physical sector size of ";
					request.m_Message += repositoryDeviceName ;
					request.m_Message += " is " ;
					request.m_Message += boost::lexical_cast<std::string>(physicalSecSize) ;
					request.m_Message += ". This is not supported. Make sure to use any disk with 512 byte sector." ;
				}
			}
			else
			{		
				errCode = E_STORAGE_PATH_NOT_FOUND;
			}
		}
#endif
		if( errCode == E_SUCCESS )
		{
			if( foundDevice.writeCacheState == VolumeSummary::WRITE_CACHE_DONTKNOW || 
				foundDevice.writeCacheState == VolumeSummary::WRITE_CACHE_ENABLED )
			{
				request.m_Message = "Disk caching status is unknown/enabled for the disk on which " ;
				request.m_Message += repositoryDeviceName ;
				request.m_Message += " exists. Disk caching should be disabled as it may cause backup location corruption in case of a unplanned shutdown of the system" ;
			}
			errCode = E_SUCCESS ;
		}
		if( errCode == E_SUCCESS )
		{
			try 
			{
				if (boost::filesystem::exists (repositoryDeviceName.c_str())) 
					errCode = E_SUCCESS;
			}
			catch (std::exception &e)
			{

			}
		}
		if( errCode == E_SUCCESS )
		{
			DebugPrintf(SV_LOG_DEBUG, "Registering the Backup location %s\n", repositoryName.c_str()) ;
			DebugPrintf(SV_LOG_DEBUG, "Backup location %s\n", repositoryDeviceName.c_str()) ;
			ConfSchemaReaderWriterPtr confSchemaReaderWriterPtr ;
			std::string repositoryPath = repositoryDeviceName ;
			if( repositoryPath.length() == 1 )
			{
				repositoryPath += ":" ;
			}
			repositoryPath += "\\" ;
			std::string configStorePathForHost = constructConfigStoreLocation( repositoryPath,  configurator.getHostId() ) ;
			if( SVMakeSureDirectoryPathExists(configStorePathForHost.c_str()) != SVS_OK )
			{
				request.m_Message = "Unable to find availability of " + 
					configStorePathForHost + 
					"Backup location not available" ;
				throw TransactionException(request.m_Message , 
					E_REPO_NOT_AVAILABLE) ;
			}
			ACE_HANDLE handle ;
			if( !isRepositoryConfiguredOnCIFS( repositoryDeviceName ) && 
				OpenVolumeExtended( &handle, repositoryPath.c_str(), GENERIC_READ ) != SVS_OK )
			{
				errCode = E_STORAGE_PATH_NOT_FOUND;
			}
			if( errCode == E_SUCCESS )
			{
				DoStopFilteringForAllVolumes() ;
				configurator.SetCDPCompression( compression ) ; 
				configurator.SetVirtualVolumeCompression( false ) ;
				ConfSchemaReaderWriter::CreateInstance( request.m_RequestFunctionName,  getRepositoryPath(repositoryPath), configStorePathForHost, false ) ;                
				VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
				volumeConfigPtr->AddRepository(repositoryName, repositoryDeviceName) ;
				confSchemaReaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
				UnmountAllVirtualVolumes() ;
				int agentRepoAccessError = -1 ;
				long agentRepoLastSuccessfulAccessTime = -1 ;
				std::string errmsg ;
#ifdef SV_WINDOWS
				UpdateAgentRepositoryAccess(-1, "") ;
#endif
				configurator.setRepositoryLocation(repositoryDeviceName) ;
				configurator.setGuestAccess(isGuestAccess) ;

				if( isRepositoryConfiguredOnCIFS( repositoryDeviceName ) )
				{
					do
					{
#ifdef SV_WINDOWS
						GetAgentRepositoryAccess(agentRepoAccessError, agentRepoLastSuccessfulAccessTime, errmsg) ;
#endif
					} while( agentRepoAccessError == -1 ) ;
					DebugPrintf(SV_LOG_DEBUG, "Access error code %d, access error %s\n", agentRepoAccessError, errmsg.c_str()) ;
					if( agentRepoAccessError != 0 )
					{
						configurator.unsetRepositoryLocation() ;
						request.m_Message = ". Backupagent service is failing to access the backup location. " + errmsg ;
						errCode = (INM_ERROR)agentRepoAccessError ;
					}
				}
				if( errCode == E_SUCCESS )
				{
					try
					{
						confSchemaReaderWriterPtr->Sync() ;
					}
					catch(TransactionException& ex)
					{
						request.m_Message = ex.what() ;
						throw ex ;
					}
				}
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR RepositoryHandler::ListConfiguredRepositories(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_INVALID_REPOSITORY ;
	bool isCorruptedFilePresent = false ;
	std::string repoName, repoPath, repostatus, writeCacheStatus  ;
	repoName = "Not Available" ;
	repostatus = "Invalid Backup Location" ;
	ConfSchema::Group dummvolgrp, dummrepogrp; 
	writeCacheStatus = "Unknown" ;
	SV_ULONGLONG capacity,quota,repoFreeSpace;
	try
	{
		getConfigStorePath() ;
	}
	catch(TransactionException &ex)
	{
		errCode = ex.getError() ;
		repoName = "Not Available" ;
		repostatus = "Not Available" ;
		writeCacheStatus = "Unknown" ;
	}
	VolumeConfigPtr volumeConfigPtr ;
	if( request.m_RequestPgs.m_Params.find("ProcessWithoutlock") == request.m_RequestPgs.m_Params.end() )
	{
		volumeConfigPtr = VolumeConfig::GetInstance() ;
		volumeConfigPtr->GetRepositoryNameAndPath(repoName, repoPath) ;

		std::set <std::string> corruptedGroups ;
		ConfSchemaMgrPtr m_confSchemaMgrPtr = ConfSchemaMgr :: GetInstance();
		isCorruptedFilePresent = m_confSchemaMgrPtr ->GetCorruptedGroups(corruptedGroups);
	}
	else
	{
		volumeConfigPtr = VolumeConfig::GetInstance(false) ;
		volumeConfigPtr->SetDummyGrps(dummvolgrp, dummrepogrp) ;
	}
	
	LocalConfigurator configurator;
	if( !repoPath.empty() )
	{
		volumeConfigPtr->GetRepositoryVolume(repoName, repoPath) ;
		try
		{
			bool status = false ;
			writeCacheStatus = "Unknown";
			std::map<std::string,volumeProperties> volpropmap;
			volumeConfigPtr->GetVolumeProperties(volpropmap);

			if( isRepositoryConfiguredOnCIFS( repoPath ) == false )
			{

				std::map<std::string,volumeProperties>::const_iterator volPropIter = volpropmap.begin();

				while(volPropIter!= volpropmap.end())
				{
					if(InmStrCmp<NoCaseCharCmp>(volPropIter->second.mountPoint,repoPath) == 0 )
					{
						capacity = boost::lexical_cast<SV_LONGLONG>(volPropIter->second.capacity);
						repoFreeSpace = boost::lexical_cast<SV_LONGLONG>(volPropIter->second.freeSpace);
						if(InmStrCmp<NoCaseCharCmp>(volPropIter->second.writeCacheState, "0") == 0 )
						{
							writeCacheStatus = "Unknown" ;
						}
						else if(InmStrCmp<NoCaseCharCmp>(volPropIter->second.writeCacheState, "1") == 0 )
						{
							writeCacheStatus = "Disabled" ;
						}
						else
						{
							writeCacheStatus = "Enabled" ;
						}
						status = true ;
						break;
					}
					volPropIter++;
				}
			}
			else
			{
				if (volpropmap.size() > 0)
					status = true ;
				writeCacheStatus = "Unknown" ;
				GetDiskFreeSpaceP (repoPath ,&quota ,&capacity,&repoFreeSpace);
			}   
			if( status )
			{
				repostatus = "Available" ;
			}
			else
			{
				repostatus = "Creation Pending" ;
			}            
			errCode = E_SUCCESS ;
		}
		catch(ContextualException& ex)
		{
			DebugPrintf(SV_LOG_DEBUG, "exception1 %s\n", ex.what()) ;
		}
		if ( isCorruptedFilePresent  )
		{
			writeCacheStatus = "Unknown" ;
			repostatus = "Data is in inconsistent state" ;
			errCode = E_SYSTEMBUSY ;
		}
	}
	else
	{
		repoPath  = configurator.getRepositoryLocation();
		if (isCorruptedFilePresent)
		{
			repostatus = "Data is in inconsistent state" ;
			errCode = E_SYSTEMBUSY ;
		}
	}
	ParameterGroup RepPG;
	ValueType vt;
	vt.value = repoName;
	RepPG.m_Params.insert(std::make_pair("RepositoryName",vt));
	vt.value = repoPath ;
	RepPG.m_Params.insert(std::make_pair("RepositoryStoragePath",vt));
	if( errCode != E_SUCCESS || repostatus == "Creation Pending" )
	{
		vt.value = "N/A" ;
	}
	else
	{
		vt.value = boost::lexical_cast<std::string>(capacity) ;
	}
	RepPG.m_Params.insert(std::make_pair("RepositoryCapacity",vt));
	if( errCode != E_SUCCESS || repostatus == "Creation Pending" )
	{
		vt.value = "N/A" ;
	}
	else
	{
		vt.value = boost::lexical_cast<std::string>(repoFreeSpace) ;
	}
	RepPG.m_Params.insert(std::make_pair("RepositoryFreeSpace",vt));
	vt.value = writeCacheStatus;
	RepPG.m_Params.insert(std::make_pair("WriteCacheStatus",vt));
	std::string username, domain, password ;
	domain = username =  password = "N/A" ;
	if( repoPath.length() > 2 && repoPath[0] == '\\' && repoPath[1] == '\\' && !configurator.isGuestAccess() )
	{
		GetCredentials(domain, username, password) ;
	}
	vt.value = username;
	RepPG.m_Params.insert(std::make_pair("UserName",vt));
	vt.value = password;
	RepPG.m_Params.insert(std::make_pair("PassWord",vt));
	vt.value = domain ;
	RepPG.m_Params.insert(std::make_pair("Domain",vt));
	if( configurator.isGuestAccess() )
	{
		vt.value = "Yes" ;
	}
	else
	{
		vt.value = "No" ;
	}
	RepPG.m_Params.insert(std::make_pair("GuestAccess",vt));
	vt.value = boost::lexical_cast<std::string>(configurator.CDPCompressionEnabled()) ;
	RepPG.m_Params.insert(std::make_pair("Compression",vt));
	ParameterGroup statusPG;
	vt.value = repostatus ;
	statusPG.m_Params.insert(std::make_pair("Status", vt));
	vt.value = boost::lexical_cast<std::string>(errCode) ;
	statusPG.m_Params.insert(std::make_pair("ReturnCode",vt));
	vt.value = "";
	statusPG.m_Params.insert(std::make_pair("ErrorMessage",vt));
	RepPG.m_ParamGroups.insert(std::make_pair("RepositoryStatus",statusPG));
	request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("ConfiguredRepository",RepPG));

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return errCode;
}

INM_ERROR RepositoryHandler::ReconstructRepo(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	LocalConfigurator lc ;
	m_progresslogfile = lc.getInstallPath() ;
	m_progresslogfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	m_progresslogfile += "bxadmin_progress.log" ;
	std::stringstream stream ;
	ConstParametersIter_t paramIter = request.m_RequestPgs.m_Params.find("All") ;
	bool all = false ;
	if( paramIter != request.m_RequestPgs.m_Params.end() &&
		paramIter->second.value == "Yes" )
	{
		all = true;
	}
	try
	{
		std::string repositoryPath = ConfSchemaMgr::GetInstance()->getRepoLocation() ;
		std::list<std::map<std::string, std::string> >hostInfoList ;
		GetHostDetailsToRepositoryConf(hostInfoList) ;
		std::list<std::map<std::string, std::string> >::const_iterator itB, itE ;
		itB = hostInfoList.begin() ;
		itE = hostInfoList.end() ;
		int i = 0 ;
		LocalConfigurator lc ;
		while( itB != itE )
		{
			const std::map<std::string, std::string>& propertyPairs = *itB ;
			i++ ;
			ParameterGroup systemGuidPg ;
			ValueType vt ;
			std::string hostname, ipaddress, agentGuid, recoveryFeaturePolicy ;
			if( propertyPairs.find("hostid") != propertyPairs.end() )
			{
				agentGuid = propertyPairs.find("hostid")->second ;
			}
			if( propertyPairs.find("hostid") != propertyPairs.end() )
			{
				hostname = propertyPairs.find("hostname")->second ;
			}
			if( !all && lc.getHostId() != agentGuid)
			{
				itB++ ;
				continue ;
			}
			std::string repoLocationForHost = constructConfigStoreLocation( repositoryPath, agentGuid ) ;
			std::string lockPath = getLockPath(repoLocationForHost) ;
			DebugPrintf(SV_LOG_INFO, "Rebuilding the configuration store for server %s\n", hostname.c_str()) ;
			stream<<"Rebuilding the configuration store for server " << hostname << std::endl ;
			ReportProgress(stream) ;
			if( lc.getHostId() != agentGuid )
			{
				RepositoryLock repoLock(lockPath) ;
				repoLock.Acquire() ;
				ConfSchemaReaderWriter::CreateInstance( request.m_RequestFunctionName, getRepositoryPath(repositoryPath), repoLocationForHost , false ) ;
				PairCreationHandler ph ;
				ph.ReconstructRepo(agentGuid) ;
				ConfSchemaReaderWriterPtr confschemardrwrtr = ConfSchemaReaderWriter::GetInstance();
				confschemardrwrtr->Sync() ;
			}
			else
			{
				ConfSchemaReaderWriter::CreateInstance( request.m_RequestFunctionName, getRepositoryPath(repositoryPath), repoLocationForHost , false ) ;
				PairCreationHandler ph ;
				ph.ReconstructRepo(agentGuid) ;
				ConfSchemaReaderWriterPtr confschemardrwrtr = ConfSchemaReaderWriter::GetInstance();
				confschemardrwrtr->Sync() ;
			}
			itB++ ;
		}
		repositoryPath = ConfSchemaMgr::GetInstance()->getRepoLocation() ;
		if (repositoryPath.empty() )
		{
			DebugPrintf(SV_LOG_INFO, "Didn't find any existing backup location.. \n") ;
			stream<<"Didn't find any existing backup location.. "<< std::endl ;
			ReportProgress(stream) ;
		}
	}
	catch(const char* x)
	{
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;

	StartSvc("backupagent") ;
	DebugPrintf(SV_LOG_INFO, "Exiting..\n") ;
	stream<<"Exiting.."<< std::endl ;
	ReportProgress(stream) ;
	return  E_SUCCESS ;
}

bool RepositoryHandler::GetCredentialsFromRequest(const FunctionInfo& request, 
												  std::string& user,
												  std::string& password,
												  std::string& domain,
												  std::string& msg)
{
	bool ret = false ;
	std::string sharepath ;
	if( request.m_RequestPgs.m_Params.find( "SharePath" ) != 
		request.m_RequestPgs.m_Params.end() )
	{
		sharepath = request.m_RequestPgs.m_Params.find( "SharePath" )->second.value ;
	}
	if( request.m_RequestPgs.m_Params.find( "RepositoryDeviceName" ) != 
		request.m_RequestPgs.m_Params.end() )
	{
		sharepath = request.m_RequestPgs.m_Params.find( "RepositoryDeviceName" )->second.value ;
	}
	do
	{
		if( request.m_RequestPgs.m_Params.find( "UserName" ) != 
			request.m_RequestPgs.m_Params.end() )
		{
			user = request.m_RequestPgs.m_Params.find( "UserName" )->second.value ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "UserName parameter is not available\n") ;
			msg = "UserName parameter is not available" ;
			break ;
		}

		if( request.m_RequestPgs.m_Params.find( "Password" ) != 
			request.m_RequestPgs.m_Params.end() )
		{
			password = request.m_RequestPgs.m_Params.find( "Password" )->second.value ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "PassWord parameter is not available\n") ;
			msg = "PassWord parameter is not available" ;
			break ;
		}
		if( request.m_RequestPgs.m_Params.find( "Domain" ) != 
			request.m_RequestPgs.m_Params.end() )
		{
			domain = request.m_RequestPgs.m_Params.find( "Domain" )->second.value ;
			if( domain.empty() || domain == "." )
			{
				domain = GetHostNameFromSharePath( sharepath ) ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Domain parameter is not available\n") ;
			msg = "Domain parameter is not available" ;
			break ;
		}
		ret = true ;
	} while( false ) ;
	return ret ;
}

INM_ERROR RepositoryHandler::UpdateCredentials(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode ;
	std::string user, password, domain, msg, sharepath ;
	bool guestAccess = false ;
	std::string errmsg ;
	bool process = false ;
	do
	{		
		if( request.m_RequestPgs.m_Params.find( "SharePath" ) != 
			request.m_RequestPgs.m_Params.end() )
		{
			sharepath = request.m_RequestPgs.m_Params.find( "SharePath" )->second.value ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "SharePath parameter is not available\n") ;
			msg = "SharePath parameter is not available" ;
			break ;
		}
		if( request.m_RequestPgs.m_Params.find( "GuestAccess" ) != 
			request.m_RequestPgs.m_Params.end() )
		{
			if( request.m_RequestPgs.m_Params.find( "GuestAccess" )->second.value == "Yes" )
			{
				DebugPrintf(SV_LOG_DEBUG, "Guest Access\n") ;
				guestAccess = true ;
			}
		}
		if( !guestAccess )
		{
			if( !GetCredentialsFromRequest(request, user, password, domain, msg) )
			{
				break ;
			}
		}
		process = true ;
	} while( false ) ;
	if( process )
	{
		if( AccessRemoteShare(sharepath, user, password, domain, guestAccess, errCode,  errmsg, true) )
		{
			/*SaveCredentialsInReg(user, password, domain, guestAccess, errCode, errmsg ) ;*/
			persistAccessCredentials(sharepath,user, password, domain,errCode,errmsg,guestAccess);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Access failed %s\n", errmsg.c_str()) ;
		}
		request.m_Message = errmsg ;
	}
	else
	{
		errCode = E_INTERNAL ;
	}
	/*LocalConfigurator lc ;
	lc.setGuestAccess( guestAccess ) ;*/
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
	StartSvc("backupagent") ;
	return errCode ;
}

std::string RepositoryHandler::getBusType(std::string devicename)
{
	unsigned int busType = retrieveBusType(devicename);
	std::string  storageBusType;

	switch(busType)
	{
	case 0x0 : storageBusType = "Unknown";
		break;
	case 0x1 : storageBusType = "SCSI";
		break;
	case 0x2 : storageBusType = "ATAPI";
		break;
	case 0x3 : storageBusType = "ATA";
		break;
	case 0x4 : storageBusType = "IEEE 1394";
		break;
	case 0x5 : storageBusType = "SSA";
		break;
	case 0x6 : storageBusType = "FiberChannel";
		break;
	case 0x7 : storageBusType = "USB";
		break;
	case 0x8 : storageBusType = "RAID";
		break;
	case 0x9 : storageBusType = "iSCSI";
		break;
	case 0xA : storageBusType = "SerialAattachedSCSI";
		break;
	case 0xB : storageBusType = "SATA";
		break;
	case 0xC : storageBusType = "SecureDigital ";
		break;
	case 0xD : storageBusType = "MultimediaCard ";
		break;
	case 0xE : storageBusType = "Virtual";
		break;
	case 0xF : storageBusType = "FileBackedVirtual";
		break;
	case 0x10 : storageBusType = "Max";
		break;
	case 0x7F : storageBusType = "MaxReserved";
		break;
	default : storageBusType = "Unknown";
		break;
	};
	return storageBusType;
}
