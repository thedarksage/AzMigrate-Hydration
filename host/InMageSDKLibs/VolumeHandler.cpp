#include "VolumeHandler.h"
#include "RepositoryHandler.h"
#include "inmstrcmp.h"
#include "inmsdkexception.h"
#include "inmageex.h"
#include "RepositoryHandler.h"
#include "volumeinfocollector.h"
#include "confengine/scenarioconfig.h"
#include "confengine/volumeconfig.h"
#include "portablehelpersmajor.h"
#include "confengine/confschemareaderwriter.h"
#include "inmsdkutil.h"
#include "APIController.h"
VolumeHandler::VolumeHandler(void)
{
}

VolumeHandler::~VolumeHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
INM_ERROR VolumeHandler::process(FunctionInfo& request)
{
	Handler::process(request);

	INM_ERROR errCode = E_SUCCESS;
	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListVolumes") == 0)
	{
		errCode = ListVolumes(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListProtectableVolumes") == 0)
	{
		errCode = ListProtectableVolumes(request);
	}
	else if (InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListProtectedVolumes") == 0)
	{
		//errCode = ListProtectedVolumes(request);
	}
	else if (InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListAvailableDrives") == 0)
	{
		errCode = ListAvailableDrives(request);
	}

	else
	{
		//Through an exception that, the function is not supported.
	}

	return Handler::updateErrorCode(errCode);
}
INM_ERROR VolumeHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}

INM_ERROR VolumeHandler::ListVolumes(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_UNKNOWN;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
    std::string reponame, repopath ;
    volumeConfigPtr->GetRepositoryNameAndPath(reponame, repopath)  ;
    if( !reponame.empty() )
    {
        if( volumeConfigPtr->isRepositoryAvailable(reponame) )
        {
            errCode = E_SUCCESS ;            
        }
        else
        {
            errCode = E_REPO_CREATION_INPROGRESS ;
        }
    }
    else
    {
        errCode = E_NO_REPO_CONFIGERED ;
    }
    if( errCode == E_SUCCESS )
    {
            std::map<std::string, volumeProperties> volumePropertiesMap ;
            volumeConfigPtr->GetVolumeProperties(volumePropertiesMap) ;
            std::map<std::string, volumeProperties>::const_iterator volpropIter ;
            volpropIter = volumePropertiesMap.begin() ;
            while( volpropIter != volumePropertiesMap.end() )
            {
                SV_ULONGLONG capacity = boost::lexical_cast<SV_ULONGLONG>(volpropIter->second.capacity) ;
				SV_INT volumeType = boost::lexical_cast<SV_INT>(volpropIter->second.isSystemVolume); 
                if( capacity )
                {
                    ParameterGroup pg ;
                    const volumeProperties& volProps = volpropIter->second ;
                    ValueType vt ;
                    vt.value = volProps.name ;
                    pg.m_Params.insert(std::make_pair("VolumeGUID",vt));
                    vt.value = volProps.name ;
                    pg.m_Params.insert(std::make_pair("VolumeName",vt));
                    vt.value = volProps.mountPoint ;
                    pg.m_Params.insert(std::make_pair("VolumeMountPoint",vt));
                    vt.value = volProps.volumeLabel ;
                    pg.m_Params.insert(std::make_pair("VolumeLabel",vt));
                    if( repopath.compare( volProps.name) == 0 )
                    {
                        vt.value = "RepositoryVolume" ;
                    }
					else if (volumeType == 1)
					{
						vt.value  = "BootVolume";
					}
                    else
                    {
                        vt.value = "General";
                    }
                    pg.m_Params.insert(std::make_pair("VolumeUsedAs",vt));
                    if( volProps.VolumeType.compare("5") == 0 )
                    {
                        vt.value = "Fixed";
                    }
                    else if( volProps.VolumeType.compare("6") == 0 )
                    {
                        vt.value = "ScoutVirtual";
                    }
                    pg.m_Params.insert(std::make_pair("VolumeType",vt)) ;
                    vt.value = volProps.capacity ;
                    pg.m_Params.insert(std::make_pair("Capacity",vt));
					vt.value = volProps.freeSpace ;
                    pg.m_Params.insert(std::make_pair("FreeSpace",vt));

                    request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(volProps.name,pg));
                    errCode = E_SUCCESS ;
                }
                volpropIter++ ;
            }
        }
    return errCode ;
}

INM_ERROR VolumeHandler::ListAvailableDrives(FunctionInfo& request)
{
	std::list<std::string> volumesOnSystem ;
	std::list<std::string> freeDriveLetters  ;
	std::string agentGuid ;
	std::string repoPath ;
	if( request.m_RequestPgs.m_Params.find( "AgentGuid" ) != request.m_RequestPgs.m_Params.end() )
	{
		agentGuid = request.m_RequestPgs.m_Params.find( "AgentGuid" )->second.value ;
	}
	if( request.m_RequestPgs.m_Params.find( "RepoPath" ) != request.m_RequestPgs.m_Params.end() )
	{
		repoPath = request.m_RequestPgs.m_Params.find( "RepoPath" )->second.value ;
	}

	LocalConfigurator configurator ;
	VolumeConfigPtr volumeConfigPtr ;
	std::string repoLocationForHost = constructConfigStoreLocation( repoPath, agentGuid ) ;
	std::string lockPath = getLockPath(repoLocationForHost) ;
	RepositoryLock repolock(lockPath) ;
	repolock.Acquire() ;
	ConfSchemaReaderWriter::CreateInstance( "ListAvailableDrives", repoPath, repoLocationForHost, false ) ;
	volumeConfigPtr = VolumeConfig::GetInstance() ;
	volumeConfigPtr->GetGeneralVolumes(volumesOnSystem) ;
	int i = 0 ;
	ValueType vt ;
	ParameterGroup availableDrives ;
	int idx = 0 ;
	for( int i = 0 ; i< 26 ; i++ )
	{
		char driveltr = 'A' + i  ;
		std::string driveltrStr  ;
		driveltrStr += driveltr ;
		if( std::find( volumesOnSystem.begin(), volumesOnSystem.end(), driveltrStr ) == volumesOnSystem.end() )
		{
			vt.value = driveltrStr ;
			std::string availableDrvIdx = "availableDrives" ;
			availableDrvIdx += "[" ;
            availableDrvIdx += boost::lexical_cast<std::string>(idx++) ;
            availableDrvIdx += "]" ;
			availableDrives.m_Params.insert( std::make_pair( availableDrvIdx , vt ) ) ;
		}
	}
	request.m_ResponsePgs.m_ParamGroups.insert( std::make_pair("availableDrives", availableDrives)) ;
	return E_SUCCESS ;
}


INM_ERROR VolumeHandler::ListProtectableVolumes(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	std::string reponame, repopath ;
	volumeConfigPtr->GetRepositoryNameAndPath(reponame, repopath)  ;
	errCode = E_SUCCESS ;
	bool bEligibleVolumes = false ;
	if( errCode == E_SUCCESS )
	{     
		std::list<std::string> volumesOnSystem ;
		volumeConfigPtr->GetGeneralVolumes(volumesOnSystem);
		if (volumesOnSystem.size() > 0)
		{
			std::list<std::string> protectedVolumes, repoVolumes ;
			std::map<std::string, volumeProperties> volumePropertiesMap ;
			volumeConfigPtr->GetVolumeProperties(volumePropertiesMap) ;
			std::map<std::string, volumeProperties>::const_iterator volpropIter ;
			scenarioConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
			volumeConfigPtr->GetRepositoryVolumes(repoVolumes) ;
			volumeConfigPtr->GetGeneralVolumes(volumesOnSystem) ;
			std::list<std::string>::const_iterator volOnSysIter ;
			volOnSysIter = volumesOnSystem.begin() ;
			errCode = E_SUCCESS ;        
			while( volOnSysIter != volumesOnSystem.end() )
			{
				volpropIter = volumePropertiesMap.find( *volOnSysIter ) ;
				if( volpropIter != volumePropertiesMap.end() )
				{                           
					if( std::find( protectedVolumes.begin(), protectedVolumes.end(), *volOnSysIter ) == protectedVolumes.end() )
					{
						if( std::find( repoVolumes.begin(), repoVolumes.end(), *volOnSysIter ) == repoVolumes.end() )
						{
							 const volumeProperties& volProps = volpropIter->second ;       
							SV_ULONGLONG ullCapacity = boost::lexical_cast<SV_ULONGLONG>( volProps.capacity ) ;
							if( ullCapacity  > 0 && volProps.VolumeType.compare("5") == 0 && ( volProps.fileSystemType == "ReFS" ||volProps.fileSystemType == "NTFS" || volProps.fileSystemType == "ExtendedFAT" ) )
							{
								bEligibleVolumes = true ;
								ParameterGroup pg ;                               
								ValueType vt ;
								vt.value = volProps.name ;
								pg.m_Params.insert(std::make_pair("VolumeGUID",vt));
								vt.value = volProps.name ;
								pg.m_Params.insert(std::make_pair("VolumeName",vt));
								vt.value = volProps.mountPoint ;
								pg.m_Params.insert(std::make_pair("VolumeMountPoint",vt));
								vt.value = volProps.volumeLabel ;
								pg.m_Params.insert(std::make_pair("VolumeLabel",vt));
								if( repopath.compare( volProps.name) == 0 )
								{
									vt.value = "RepositoryVolume" ;
								}
								else  if( volProps.isSystemVolume.compare("1") == 0 || 
										volProps.isBootVolume.compare("1") == 0 )
								{
									vt.value = "BootVolume" ;
								}
								else if( InmStrCmp<NoCaseCharCmp>( volProps.mountPoint, "C:\\SRV" ) == 0 )
								{
									vt.value = "SystemVolume" ;
								}
								else
								{
									 vt.value = "General";
								}
								pg.m_Params.insert(std::make_pair("VolumeUsedAs",vt));
								if( volProps.VolumeType.compare("5") == 0 )
								{
									vt.value = "Fixed";
								}
								else if( volProps.VolumeType.compare("6") == 0 )
								{
									vt.value = "ScoutVirtual";
								}
								pg.m_Params.insert(std::make_pair("VolumeType",vt)) ;
								vt.value = volProps.capacity ;
								pg.m_Params.insert(std::make_pair("Capacity",vt));
								request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(volProps.name,pg));      
							}
						}
					}                   
				}
				volOnSysIter++ ;
			}
			
		}
		else 
		{
#ifdef SV_WINDOWS
			mountBootableVolumesIfRequried() ;
#endif
			RepositoryHandler repoHandler;
			std::map<std::string,VolumeDetails> eligibleVolumes ;
			repoHandler.GetEligibleVolumes(eligibleVolumes)  ;
			std::map<std::string,VolumeDetails>::const_iterator eligibleVolumesIter ;
			eligibleVolumesIter = eligibleVolumes.begin() ;
			while( eligibleVolumesIter != eligibleVolumes.end() )
			{
				bEligibleVolumes = true ;
				ParameterGroup deviceNamePg ;
				ValueType vt ;
				vt.value = eligibleVolumesIter->first ;
				deviceNamePg.m_Params.insert(std::make_pair("VolumeName", vt)) ;
				vt.value = eligibleVolumesIter->first ;
				deviceNamePg.m_Params.insert(std::make_pair("VolumeGUID", vt)) ;
				vt.value = boost::lexical_cast<std::string>(eligibleVolumesIter->second.capacity) ;
				deviceNamePg.m_Params.insert(std::make_pair("Capacity", vt)) ;
				if (eligibleVolumesIter->second.writeCacheState == 0)
					vt.value = "Unknown";
				else if(eligibleVolumesIter->second.writeCacheState == 1)
					vt.value = "Disabled";
				else 
					vt.value = "Enabled";
				deviceNamePg.m_Params.insert(std::make_pair("DiskCacheStatus",vt));
				vt.value = boost::lexical_cast<std::string>(eligibleVolumesIter->second.freeSpace) ;
				deviceNamePg.m_Params.insert(std::make_pair("VolumeFreeSpace", vt)) ;
				if( eligibleVolumesIter->second.isSystemVolume == true || eligibleVolumesIter->second.volumeLabel == "System Reserved" ) 
				{
					vt.value = "BootVolume" ;
					if( InmStrCmp<NoCaseCharCmp>( eligibleVolumesIter->second.mountPoint , "C:\\SRV\\" ) == 0 || eligibleVolumesIter->second.volumeLabel == "System Reserved" )
					{
						vt.value = "SystemVolume" ;
					}
				}
				else
				{
					vt.value = "General";
				}
				deviceNamePg.m_Params.insert(std::make_pair("VolumeUsedAs", vt)) ;
				vt.value = eligibleVolumesIter->second.mountPoint; 
				deviceNamePg.m_Params.insert(std::make_pair("VolumeMountPoint", vt)) ;

				vt.value = eligibleVolumesIter->second.volumeLabel; 
				deviceNamePg.m_Params.insert(std::make_pair("VolumeLabel", vt)) ;

				request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(eligibleVolumesIter->first, deviceNamePg)) ;                
				eligibleVolumesIter++ ;
			}

		}
	}
    if( !bEligibleVolumes )
        errCode = E_NO_VOLUMES_FOUND_FOR_SYSTEM ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return errCode ;
}
