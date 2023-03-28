#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <boost/lexical_cast.hpp>
#include "paircreationhandler.h"
#include "confschemamgr.h"
#include "policyhandler.h"
#include "RepositoryHandler.h"
#include "inmstrcmp.h"
#include "apinames.h"
#include "util.h"
#include "confengine/scenarioconfig.h"
#include "confengine/volumeconfig.h"
#include "confengine/policyconfig.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/alertconfig.h"
#include "confengine/auditconfig.h"
#include "confengine/agentconfig.h"
#include "confengine/eventqueueconfig.h"
#include "portablehelpersmajor.h"
#include "initialsettings.h"
#include "inmsdkutil.h"
PairCreationHandler::PairCreationHandler(void):m_pPlanGroup(0),m_pProtectionPairGroup(0)
{
}
PairCreationHandler::~PairCreationHandler(void)
{
}

bool PairCreationHandler::CreatePairs(SV_UINT batchSize)
{
    ScenarioConfigPtr scenarioConfigPtr ;
    VolumeConfigPtr volumeConfigPtr ;
    ProtectionPairConfigPtr protectionpairConfigPtr ;
    INM_ERROR errCode = E_UNKNOWN ;
    scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    protectionpairConfigPtr = ProtectionPairConfig::GetInstance() ;
    bool bSync = false;
    if( scenarioConfigPtr->ProtectionScenarioAvailable() )
    {
        std::list<std::string> protectedVolumes ;
        std::map<std::string, PairDetails> pairDetailsMap ;
        std::string executionStatus ;
        scenarioConfigPtr->GetExecutionStatus(executionStatus) ;

        if( scenarioConfigPtr->GetPairDetails(pairDetailsMap) )
        {
            std::map<std::string, PairDetails>::iterator pairDetailIter ;
            std::list<std::string> protectedVolumesFromScenario, protectedVolumesFromProtection ;
            pairDetailIter = pairDetailsMap.begin() ;
            while( pairDetailIter != pairDetailsMap.end() )
            {
                protectedVolumesFromScenario.push_back( pairDetailIter->first ) ;
                if( !protectionpairConfigPtr->IsPairExists(pairDetailIter->first, pairDetailIter->second.targetVolume) )
                {
                    DebugPrintf(SV_LOG_DEBUG, "pair %s <-> %s doesn't exists\n", 
                        pairDetailIter->first.c_str(), pairDetailIter->second.targetVolume.c_str()) ;                  
                    volumeConfigPtr = VolumeConfig::GetInstance() ;
                    if( volumeConfigPtr->IsVolumeAvailable(pairDetailIter->first) )
                    {
                        if( volumeConfigPtr->IsTargetVolumeAvailable(pairDetailIter->second.targetVolume) )
                        {
                            bSync = true ;                 
                            protectionpairConfigPtr->AddToProtection(pairDetailIter->second, batchSize) ; 
                            PolicyConfigPtr policyConf = PolicyConfig::GetInstance() ; 
                            if( policyConf->isConsistencyPolicyEnabled() )
                            {
                                ConsistencyOptions consitencyOpts ;
                                scenarioConfigPtr->GetConsistencyOptions( consitencyOpts )  ;
                                std::list<std::string> protectedVolumes ;
                                protectionpairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
                                policyConf->UpdateConsistencyPolicy( consitencyOpts, protectedVolumes ) ;   
                            }
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "Cannot find the properties of the %s from volume config\n",
                                pairDetailIter->second.targetVolume.c_str()) ;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "Cannot find the properties of the %s from volume config\n",
                            pairDetailIter->first.c_str()) ;
                    }                   
                }
                pairDetailIter++ ;
            }
            protectionpairConfigPtr->GetProtectedVolumes(protectedVolumesFromProtection) ;         
            if( protectedVolumesFromProtection.size() == protectedVolumesFromScenario.size() )
            {
                scenarioConfigPtr->SetExecutionStatus(UNDER_PROTECTION) ;
                PolicyConfigPtr policyConfigPtr ;
                policyConfigPtr = PolicyConfig::GetInstance() ;
                policyConfigPtr->EnableConsistencyPolicy() ;
                bSync = true ;
            }
        }
    }
    return bSync ;
}

INM_ERROR PairCreationHandler::process(FunctionInfo& f)
{
    Handler::process(f);
    INM_ERROR errCode = E_SUCCESS;
    if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName,ENABLE_VOLUME_UNPROVISIONING) == 0)
    {
        errCode = EnableVolumeUnprovisioningPolicy(f);
    }
    else if (InmStrCmp<NoCaseCharCmp> (m_handler.functionName,MONITOR_EVENTS) == 0)
    {
        errCode = MonitorEvents (f);
    }
	else if (InmStrCmp<NoCaseCharCmp> (m_handler.functionName,PENDING_EVENTS) == 0)
	{
		errCode = PendingEvents(f) ;
	}
    return Handler::updateErrorCode(errCode);
}
INM_ERROR PairCreationHandler::validate(FunctionInfo& request)
{
    INM_ERROR errCode = Handler::validate(request);
    if(E_SUCCESS != errCode)
        return errCode;

    //TODO: Write hadler specific validation here

    return errCode;
}

std::list <std::string> PairCreationHandler::getVolumesFromVolumeUnprovisioningPolicy (FunctionInfo &f)
{
    std::list <std::string> volumeList ;
    ParameterGroupsIter_t paramGroupsIter = f.m_RequestPgs.m_ParamGroups.begin();
    ParametersIter_t paramIter = paramGroupsIter->second.m_Params.begin() ; 
    while (paramIter != paramGroupsIter->second.m_Params.end() ) 
    {
        volumeList .push_back (paramIter->second.value);
        paramIter ++;
    }
    return volumeList ;
}



INM_ERROR PairCreationHandler::EnableVolumeUnprovisioningPolicy(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    INM_ERROR errCode = E_UNKNOWN;
    std::list <std::string> volumeListToDeletion ; 
    std::list <std::string> volumeListMarkedForDeletion;
    std::list <std::string>::iterator volumeListFromRequestIter ; 
    std::list <std::string> volumeListFromRequest ; 
    PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    ProtectionPairConfigPtr protectionpairconfigptr ;
    protectionpairconfigptr = ProtectionPairConfig::GetInstance() ;   
    VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
    volumeListFromRequest = getVolumesFromVolumeUnprovisioningPolicy (f);

    protectionpairconfigptr->GetVolumesMarkedForDeletion (volumeListMarkedForDeletion);
    std::list <std::string>::iterator volumeListMarkedForDeletionIter = volumeListMarkedForDeletion.begin();	
    while (volumeListMarkedForDeletionIter != volumeListMarkedForDeletion.end())
    {
        volumeListFromRequestIter = find (volumeListFromRequest.begin(),volumeListFromRequest.end(),
            *volumeListMarkedForDeletionIter);
        if (volumeListFromRequestIter == volumeListFromRequest.end())
        {
            volumeListToDeletion.push_back (*volumeListMarkedForDeletionIter); 
            DebugPrintf (SV_LOG_DEBUG ,"Volume to delete is  %s \n " , volumeListMarkedForDeletionIter->c_str());
        }
        volumeListMarkedForDeletionIter ++; 
    }
    bool procede = scenarioConfigPtr->GetTargetVolumeUnProvisioningStatus (volumeListToDeletion); 
    if (procede == true && volumeListToDeletion.size() > 0)
    {
        DebugPrintf(SV_LOG_DEBUG, " Enter in to volumeListToDeletion.size() > 0 \n ") ;

        std::map<std::string, std::string> srcTgtVolMap ;
        std::map<std::string, volumeProperties> volumePropertiesMap ;
        if( volumeConfigPtr->GetVolumeProperties( volumePropertiesMap ) )
        {
            int scenarioId = scenarioConfigPtr->GetProtectionScenarioId() ;
            scenarioConfigPtr->GetSourceTargetVoumeMapping( volumeListToDeletion, srcTgtVolMap, true ) ;
            if( !srcTgtVolMap.empty() )
            {
                policyConfigPtr->CreateVolumeUnProvisioningPolicy(srcTgtVolMap, 
                    volumePropertiesMap,
                    scenarioId) ; 
                scenarioConfigPtr->SetTargetVolumeUnProvisioningStatus (volumeListToDeletion);
                policyConfigPtr->EnableVolumeUnProvisioningPolicy() ;
				EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
				eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

				ConfSchemaReaderWriterPtr confschemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
				confschemaReaderWriter->Sync();
            }
        }
    }

    insertintoreqresp(f.m_ResponsePgs, cxArg ( 1 ), "1");
    errCode = E_SUCCESS ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

bool PairCreationHandler::DetectVolumeResize()
{
	DebugPrintf  (SV_LOG_DEBUG, "ENTERED %s \n", __FUNCTION__ ); 
    ProtectionPairConfigPtr protectionpairconfigptr ;
    VolumeConfigPtr volumeConfigPtr ;
    protectionpairconfigptr = ProtectionPairConfig::GetInstance() ;
    volumeConfigPtr = VolumeConfig::GetInstance() ;
    std::list<std::string> protectedVolumes ;
    bool detect = false ;
    protectionpairconfigptr->GetProtectedVolumes(protectedVolumes) ;
    std::list<std::string>::const_iterator protectedVolIter ;
    protectedVolIter = protectedVolumes.begin() ;
    while( protectedVolIter != protectedVolumes.end() )
    {
        SV_LONGLONG pairInsertionTime ;
        SV_LONGLONG latestResizeTs ;
		VOLUME_SETTINGS::PAIR_STATE pairState ;
		protectionpairconfigptr->GetPairState(*protectedVolIter, pairState ) ;
        if( !protectionpairconfigptr->IsPairMarkedAsResized(*protectedVolIter) && 
            protectionpairconfigptr->GetPairInsertionTime(*protectedVolIter, pairInsertionTime)&&
			!( pairState == VOLUME_SETTINGS::DELETE_PENDING || pairState == VOLUME_SETTINGS::CLEANUP_DONE)) 
        {
            if( volumeConfigPtr->GetLatestResizeTs(*protectedVolIter, latestResizeTs) )
            {
                if( pairInsertionTime < latestResizeTs )
                {
                    detect = true ;
                    protectionpairconfigptr->MarkPairAsResized(*protectedVolIter) ;
                    protectionpairconfigptr->PauseProtection(*protectedVolIter) ;
                }
            }
        }
        protectedVolIter++ ;
    }
	DebugPrintf (SV_LOG_DEBUG, "Exited %s \n", __FUNCTION__) ; 
    return detect ;
}

bool PairCreationHandler::GetResizedVolume(std::string &resizedVolumeName)
{
	DebugPrintf  (SV_LOG_DEBUG, "ENTERED %s \n", __FUNCTION__ ); 
    ProtectionPairConfigPtr protectionpairconfigptr ;
    VolumeConfigPtr volumeConfigPtr ;
    protectionpairconfigptr = ProtectionPairConfig::GetInstance() ;
    volumeConfigPtr = VolumeConfig::GetInstance() ;
    std::list<std::string> protectedVolumes ;
    bool detect = false ,firstTime = true;
    protectionpairconfigptr->GetProtectedVolumes(protectedVolumes) ;
    std::list<std::string>::const_iterator protectedVolIter ;
    protectedVolIter = protectedVolumes.begin() ;
	int previousVolumeResizeActionCount = 0, currentVolumeResizeActionCount = 0; 
    while( protectedVolIter != protectedVolumes.end() )
    {
		VOLUME_SETTINGS::PAIR_STATE pairState ;
		protectionpairconfigptr->GetPairState(*protectedVolIter, pairState ) ;
        if( protectionpairconfigptr->IsPairMarkedAsResized(*protectedVolIter) && 
			( pairState != VOLUME_SETTINGS::DELETE_PENDING || pairState != VOLUME_SETTINGS::CLEANUP_DONE)) 
        {
			detect = true;
			protectionpairconfigptr->GetVolumeResizeActionCount (*protectedVolIter , currentVolumeResizeActionCount );  
			DebugPrintf (SV_LOG_DEBUG, "VolumeName is %s currentVolumeResizeActionCount is %d \n" , protectedVolIter->c_str(),currentVolumeResizeActionCount ); 
			if (firstTime == true ) 
			{
				previousVolumeResizeActionCount = currentVolumeResizeActionCount; 
				firstTime = false; 
			}
			DebugPrintf (SV_LOG_DEBUG, "previousVolumeResizeActionCount is %d currentVolumeResizeActionCount is %d \n" , previousVolumeResizeActionCount,currentVolumeResizeActionCount ); 

			if (previousVolumeResizeActionCount >= currentVolumeResizeActionCount)
			{
				resizedVolumeName = protectedVolIter->c_str() ;
				previousVolumeResizeActionCount = currentVolumeResizeActionCount; 
				DebugPrintf (SV_LOG_DEBUG , "resizedVolumeName is %s  \n ", resizedVolumeName.c_str()); 
			}
        }
        protectedVolIter++ ;
    }
	DebugPrintf (SV_LOG_DEBUG, "Exited %s \n", __FUNCTION__) ; 
    return detect ;
}

bool PairCreationHandler::ThresholdExceededAlert()
{
    ProtectionPairConfigPtr protectionPairConfigPtr ;
    bool bRet = false ;
    protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::list<std::string> protectedVolumes ;
    protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
    std::list<std::string>::const_iterator protectedVolIter ;
    protectedVolIter = protectedVolumes.begin() ;
    AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
    while( protectedVolIter != protectedVolumes.end() )
    {
        double rpoInSec = 0 ; 
		if( protectionPairConfigPtr->GetRPOInSeconds(*protectedVolIter, rpoInSec) )
        {
            if( rpoInSec > 30 * 60 )
            {
                bRet = true ;
                std::string rpoInSecStr = protectionPairConfigPtr->ConvertRPOToHumanReadableformat (rpoInSec );
                std::string alertPrefix = "Current RPO time for " +  *protectedVolIter ;

                std::string alertMsg = alertPrefix +  " is " + rpoInSecStr;
                alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg, alertPrefix) ;  // Error Level chaned to WARNING
            }
        }
        protectedVolIter++ ;
    }
    return bRet ;
}

INM_ERROR PairCreationHandler::PendingEvents (FunctionInfo &f )
{
	std::string eventsPresent = "0" ;
	EventQueueConfigPtr eventQueueConfigPtr = EventQueueConfig::GetInstance() ;
	if( eventQueueConfigPtr->AnyPendingEvents() )
	{
		eventsPresent = "1" ;
		eventQueueConfigPtr->ClearPendingEvents() ;
	}
	if( eventsPresent == "1" )
	{
		ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
		confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
		confschemareaderwriterPtr->Sync() ;
	}
    insertintoreqresp(f.m_ResponsePgs, cxArg ( eventsPresent ), "1" );
    return E_SUCCESS ;
}

INM_ERROR PairCreationHandler::MonitorEvents (FunctionInfo &f )
{
    INM_ERROR errCode = E_UNKNOWN ;
    bool anyEvents = false ;
    if( DetectVolumeResize() )
    {
        anyEvents = true ;
    }
	std::string  resizedVolume; 
	
	if ( GetResizedVolume (resizedVolume)) 
	{
		anyEvents = true ;
		VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance(); 
		ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
		if (!isVsnapsMounted()) 
		{
			if (protectionPairConfigPtr->isVolumePaused(resizedVolume)) 
			{
				std::string msg ;
				std::map <std::string,hostDetails> hostDetailsMap ; 
				if (CheckSpaceAvailabilityForResizedVolume (resizedVolume,msg)) 
				{
					protectionPairConfigPtr->validateAndActOnResizedVolume (resizedVolume);  
				}
				else 
				{
					// need to send alerts
					//std::string alertMsg = FormatSpaceCheckMsg("",hostDetailsMap ) ;
					AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
					std::string alertPrefix ;
					alertConfigPtr->processAlert(msg,alertPrefix); 
					alertConfigPtr->AddAlert("FATAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, msg,alertPrefix) ;
				}
			}
		}
		else
		{
			ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
			protectionPairConfigPtr->IncrementVolumeResizeActionCount(resizedVolume); 
			std::string alertMsg = "Vsnaps are mounted in the system.Please unmount the Vsnaps to continue volume resize operation on ";
			alertMsg += resizedVolume.c_str(); 
			AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
			alertConfigPtr->AddAlert("FATAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;
		}
	}
    if( TriggerAutoResync() )
    {
        anyEvents = true ;
    }
    if( DeletePairs() )
    {
        anyEvents = true ;
    }
	if( CheckForPendingResumeTracking() )
	{
		anyEvents = true ;
	}
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    SV_UINT batchSize = scenarioConfigPtr->GetBatchSize() ;
    if( CreatePairs(batchSize) )
    {
        anyEvents = true ;
    }
    if( anyEvents )
    {
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

        ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
        confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        confschemareaderwriterPtr->Sync() ;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    return E_SUCCESS ;
}
/*
TODO : NOT COMPLETE YET
*/
bool PairCreationHandler::TriggerAutoResync()
{
    ScenarioConfigPtr scenarioConfigPtr ;
    ProtectionPairConfigPtr protectionPairConfigPtr ;
    bool bReturnCode = false ;
    scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    if( scenarioConfigPtr->CanTriggerAutoResync() )
    {
        SV_UINT batchSize = scenarioConfigPtr->GetBatchSize() ;
        protectionPairConfigPtr->PerformAutoResync(batchSize) ;
        bReturnCode = true ;
    }
    return bReturnCode; 
}

bool PairCreationHandler::CheckForPendingResumeTracking() 
{
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	return protectionPairConfigPtr->PerformPendingResumeTracking() ;
}

bool PairCreationHandler::DeletePairs()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bReturnCode = false ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::string executionStatus ;
    bReturnCode = protectionPairConfigPtr->IsCleanupDoneForPairs( );
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bReturnCode ;
}


bool PairCreationHandler::DeleteCorruptedConfigFiles()
{
	std::string progresslogfile ;
	LocalConfigurator lc ;
	RepositoryHandler rh ;
	rh.m_progresslogfile = lc.getInstallPath() ;
	rh.m_progresslogfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	rh.m_progresslogfile += "bxadmin_progress.log" ;
	std::stringstream stream ;
	
    std::string configDir = ConfSchemaMgr::GetInstance()->getConfigDirLocation() ;
    ACE_DIR * ace_dir = NULL ;
    ACE_DIRENT * dirent = NULL ;
    bool isCorruptedVolumeConfigPresent  = false ;
    bool isVolumeConfigPresent = false ;
    bool corruptPolicyConfig = false ;
    bool corruptedConfig = false ;
    bool canRepair = true ;
    ace_dir = sv_opendir(configDir.c_str()) ;
    if( ace_dir != NULL )
    {
        DebugPrintf(SV_LOG_INFO, "\n\nInspecting the configuration store contents\n\n") ;
		stream<< "\nInspecting the configuration store contents\n "<< std::endl ;
		rh.ReportProgress(stream) ;
        do
        {
            dirent = ACE_OS::readdir(ace_dir) ;
            bool isDir = false ;
            if( dirent != NULL )
            {
                std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
                if( strcmp(fileName.c_str(), "." ) == 0 || strcmp(fileName.c_str(), ".." ) == 0 )
                {
                    continue ;
                }
                if( fileName.find(".corrupt") != std::string::npos )
                {
                    corruptedConfig = true ;
                    std::string configdatatype ;
                    std::string grpType ;
                    if( fileName.find("agent_config") != std::string::npos )
                    {
                        configdatatype = "Agent configuration data" ;
                        grpType =  AGENTCONFIG_GROUP;
                    }
                    else if( fileName.find( "alert_config") != std::string::npos )
                    {
                        configdatatype = "Alert configuration data" ;
                        grpType =  ALERTS_GROUP;
                    }
                    else if( fileName.find( "audit_config") != std::string::npos )
                    {
                        configdatatype = "Audit configuration data" ;
                        grpType =  AUDITCONFIG_GROUP;
                    }
                    else if( fileName.find( "protectionpair_config") != std::string::npos )
                    {
                        configdatatype = "Protection configuration data" ;
                        grpType =  PROTECTIONPAIRS_GROUP;
                    }

                    else if( fileName.find( "policy_config") != std::string::npos )
                    {
                        configdatatype = "Policy configuration data" ;
                        grpType =  POLICY_GROUP ;
                    }
                    else if( fileName.find( "policyinstance_config") != std::string::npos )
                    {
                        configdatatype = "Policy instance configuration data" ;
                        grpType =  POLICY_INSTANCE_GROUP;
                    }
                    else if( fileName.find("scenario_config") != std::string::npos )
                    {
                        configdatatype = "Scenario configuration data" ;
                        grpType = PLAN_GROUP ;
                    }
                    else if( fileName.find("volumes_config") != std::string::npos )
                    {
                        configdatatype = "Volume configuration data" ;
                        grpType =  VOLUME_GROUP;
                    }

                    DebugPrintf(SV_LOG_INFO, "\t%s is in inconsistent state.\n\n", configdatatype.c_str() ) ;
					stream<< "\t"<<configdatatype<<" is in inconsistent state. "<< std::endl ;
					rh.ReportProgress(stream) ;
                    std::string corruptfile = configDir ;
                    corruptfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
                    corruptfile += fileName ;
                    if( fileName.find("policy") != std::string::npos )
                    {
                        corruptPolicyConfig = true ;
                    }
                    if( fileName.find("scenario_config") != std::string::npos ||
                        fileName.find("agent_config") != std::string::npos ||
                        fileName.find("volumes_config") != std::string::npos )
                    {
                        std::list<std::string> bkpfiles ;
                        std::string bkpfile ;
                        bkpfiles = ConfSchemaMgr::GetInstance()->GetSchemaFilePathForBkp( grpType ) ;
                        std::list<std::string>::const_iterator bkpfileiter ;
                        bkpfileiter = bkpfiles.begin() ;
                        if( bkpfiles.size() )
                        {
                            DebugPrintf(SV_LOG_INFO, "\tTrying to restore the %s from backup copies..\n\n", configdatatype.c_str()) ;
							stream<< "\tTrying to restore the "<<configdatatype<<" from backup copies.."<< std::endl ;
							rh.ReportProgress(stream) ;
                        }
                        canRepair = false ;
                        while( bkpfileiter != bkpfiles.end() )
                        {
                            try
                            {
                                std::string absPath = corruptfile ;
                                boost::algorithm::replace_all( absPath, ".corrupt", "") ;
                                if( boost::filesystem::exists( *bkpfileiter ) )
                                {
                                    if( boost::filesystem::exists(absPath.c_str() ) )
                                    {
                                        ACE_OS::unlink( absPath.c_str() ) ;
                                    }
                                    boost::filesystem::copy_file( *bkpfileiter, absPath.c_str()) ;
                                    if( grpType == PLAN_GROUP )
                                    {
                                        ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
                                    }
                                    else if( grpType == AGENTCONFIG_GROUP )
                                    {
                                        AgentConfigPtr agentConfigptr = AgentConfig::GetInstance() ;
                                    }
                                    else if( grpType == VOLUME_GROUP )
                                    {
                                        VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
                                    }
                                    DebugPrintf(SV_LOG_INFO, "\tSuccessfully restored the %s..\n\n", configdatatype.c_str()) ;
									stream<< "\tSuccessfully restored the "<<configdatatype<<".."<< std::endl ;
									rh.ReportProgress(stream) ;
                                    canRepair = true ;
                                    break ;
                                }
                                else
                                {
                                    DebugPrintf(SV_LOG_INFO, "\tThe backup file %s doesn't exist\n\n", bkpfileiter->c_str()) ;
									stream<< "\tThe backup file "<< bkpfileiter->c_str() <<" doesn't exist"<< std::endl ;
									rh.ReportProgress(stream) ;
                                }
                            }
                            catch( TransactionException& te1)
                            {
                                DebugPrintf(SV_LOG_INFO, "\tThe backup file %s is not well formatted. Cannot use it...\n\n", bkpfileiter->c_str()) ;
								stream<< "\tThe backup file "<< bkpfileiter->c_str() <<" is not well formatted. Cannot use it..."<< std::endl ;
								rh.ReportProgress(stream) ;
                                canRepair = false ;
                            }
                            bkpfileiter++ ;
                        }
                        if( canRepair )
                        {
                            ACE_OS::unlink( corruptfile.c_str() ) ;
                        }
                    }
                    else
                    {
                        ACE_OS::unlink( corruptfile.c_str() ) ;
                    }
                }
            }
        } while ( dirent != NULL  ) ;
    }
    if( !canRepair )
    {
        DebugPrintf(SV_LOG_INFO, "\n\nCannot continue with rebuilding of configstore..\n") ;
		stream<< "\nCannot continue with rebuilding of configstore.."<< std::endl ;
		rh.ReportProgress(stream) ;
    }
    if( corruptPolicyConfig )
    {
        std::string policyconfigfile = configDir ;
        policyconfigfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
        policyconfigfile += "policy_config.xml" ;
        if( boost::filesystem::exists( policyconfigfile.c_str() ) )
        {
            DebugPrintf(SV_LOG_INFO, "\tRemoving the policy configuration data before re-building it\n") ;
			stream<< "\tRemoving the policy configuration data before re-building it"<< std::endl ;
			rh.ReportProgress(stream) ;
            ACE_OS::unlink( policyconfigfile.c_str()) ;
        }
        policyconfigfile = configDir ;
        policyconfigfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
        policyconfigfile += "policyinstance_config.xml" ;
        if( boost::filesystem::exists( policyconfigfile.c_str() ) )
        {
            DebugPrintf(SV_LOG_INFO, "\tRemoving the policy instance configuration data before re-building it\n") ;
			stream<< "\tRemoving the policy instance configuration data before re-building it"<< std::endl ;
			rh.ReportProgress(stream) ;
            ACE_OS::unlink( policyconfigfile.c_str()) ;
        }
        PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
        policyConfigPtr->ClearPolicies();
    }
    if( !canRepair )
    {
        throw "Cannot continue with repair activity" ;
    }
    if( corruptedConfig )
    {
        DebugPrintf(SV_LOG_INFO, "Rebuilding the remaining configuration data..\n") ; 
		stream<< "Rebuilding the remaining configuration data.."<< std::endl ;
		rh.ReportProgress(stream) ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "Nothing to rebuild..\n") ; 
		stream<< "Nothing to rebuild.."<< std::endl ;
		rh.ReportProgress(stream) ;

    }
    return corruptedConfig ;
}

void PairCreationHandler::updatePairStateFromLocalSettings(HOST_VOLUME_GROUP_SETTINGS& hvs)
{
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::list<std::string> protectedvolumes ;
    protectionPairConfigPtr->GetProtectedVolumes(protectedvolumes) ;
    std::list<std::string>::const_iterator protectedVolIter ;
    protectedVolIter = protectedvolumes.begin() ;
    while( protectedVolIter != protectedvolumes.end() )
    {
        if( hvs.isProtectedVolume(*protectedVolIter) )
        {
            VOLUME_SETTINGS::SYNC_MODE syncMode = VOLUME_SETTINGS::SYNC_DIFF;
            if( hvs.isResyncing(*protectedVolIter) )
            {
                if( hvs.isInResyncStep1(*protectedVolIter) )
                {
                    syncMode = VOLUME_SETTINGS::SYNC_DIRECT ;                    
                }
                else
                {
                    syncMode = VOLUME_SETTINGS::SYNC_QUASIDIFF ;                    
                }
            }
			VOLUME_SETTINGS::PAIR_STATE pairState = hvs.getPairState (*protectedVolIter); 
			protectionPairConfigPtr->UpdatePairState (*protectedVolIter, pairState);
            protectionPairConfigPtr->UpdateReplState( *protectedVolIter, syncMode ) ;
            protectionPairConfigPtr->SetQueuedState( *protectedVolIter, PAIR_IS_NONQUEUED ) ;
        }
        else
        {
            protectionPairConfigPtr->UpdateReplState( *protectedVolIter, VOLUME_SETTINGS::SYNC_DIRECT ) ;
            protectionPairConfigPtr->SetQueuedState( *protectedVolIter, PAIR_IS_QUEUED ) ;
        }
        protectedVolIter++ ;
    }
}
void PairCreationHandler::CreatePairsIfRequired()
{
    SV_UINT batchSize = 1 ;
    try
    {
        ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
        batchSize = scenarioConfigPtr->GetBatchSize() ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Using the batch size as 1 as the configured batch size is not known\n") ;
    }
    CreatePairs(batchSize) ;
    HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings = m_Configurator->getHostVolumeGroupSettings();
    if( hostVolumeGroupSettings.volumeGroups.size() )
    {
        updatePairStateFromLocalSettings(hostVolumeGroupSettings) ;
    }    
}

void PairCreationHandler::ReconstrProtectionPairConfig( )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	bool protectionPairCorrupt = false;
    std::list<std::string> protectedVolumesInProtectionConfigData, protectedVolumesInScenarioCOnfigData ;
    protectionPairConfigPtr->GetProtectedVolumes( protectedVolumesInProtectionConfigData ) ;
    scenarioConfigPtr->GetProtectedVolumes( protectedVolumesInScenarioCOnfigData ) ; 
    if( protectedVolumesInScenarioCOnfigData.size() && !protectedVolumesInProtectionConfigData.size() )
    {
        CreatePairsIfRequired() ;
        HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings = m_Configurator->getHostVolumeGroupSettings();
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd;
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t endPointVolGrpSettings;

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIter = hostVolumeGroupSettings.volumeGroups.begin();

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEnd =  hostVolumeGroupSettings.volumeGroups.end();
        ProtectionPairConfigPtr protectionPairConfigPtr ;
        protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
        for (/* empty */; volumeGroupIter != volumeGroupEnd; ++volumeGroupIter) 
        {
            volumeIter = (*volumeGroupIter).volumes.begin();
            volumeEnd = (*volumeGroupIter).volumes.end();
            for (/* empty*/; volumeIter != volumeEnd; ++volumeIter) 
            {
                PairDetails pairDetails ;

                std::string deviceName = (*volumeIter).second.deviceName;
                std::string targetVol = (*volumeIter).second.endpoints.begin()->mountPoint ;
                std::string catalogpath = m_Configurator->getCdpDbName(targetVol) ;
                protectionPairConfigPtr->SetResyncRequiredBySource( deviceName, 
                    "Reconstructing the pair settings") ;
                scenarioConfigPtr->GetPairDetails(deviceName, pairDetails ) ;
                protectionPairConfigPtr->UpdatePairWithRetention( deviceName,
                    pairDetails.retentionVolume,
                    pairDetails.retentionPath,
                    catalogpath,
                    pairDetails.retPolicy ) ;
				protectionPairCorrupt = true;
            }
        }
    }
	if (protectionPairCorrupt == true)
	{
		std::cout << "\tSuccessfully restored the Protection configuration data.." << std::endl;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void PairCreationHandler::ReconstructPolicyConfig()
{
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
    if( scenarioConfigPtr->ProtectionScenarioAvailable() && !policyConfigPtr->PoliciesAvailable() )
    {
        int scenarioId = scenarioConfigPtr->GetProtectionScenarioId() ;
        std::list<std::string> protectedVols ;
        scenarioConfigPtr->GetProtectedVolumes( protectedVols ) ;
        ConsistencyOptions consistencyOptions ;
        scenarioConfigPtr->GetConsistencyOptions(consistencyOptions) ;
        policyConfigPtr->CreateConsistencyPolicy(consistencyOptions, protectedVols, scenarioId, true, true) ;
    }
}
void PairCreationHandler::LoadAllConfigurationFiles()
{
    try {
        AgentConfig::GetInstance() ;
    }
    catch(TransactionException& te1) {
    }
    try {
        AlertConfig::GetInstance() ;
    }
    catch(TransactionException& te1) {
    }
    try {
        AuditConfig::GetInstance() ;
    }
    catch(TransactionException& te1) {
    }
    try {
        PolicyConfig::GetInstance() ;
    }
    catch(TransactionException& te1) {
    }
    try {
        ProtectionPairConfig::GetInstance() ;
    }
    catch(TransactionException& te1) {
    }
    try {
        ScenarioConfig::GetInstance() ;
    }
    catch(TransactionException& te1) {

    }
    try {
        VolumeConfig::GetInstance() ;
    }
    catch(TransactionException& te1) {
    }
    ConfSchemaMgr::GetInstance()->Reset() ;
    ConfSchemaReaderWriterPtr confschemawrtrptr = ConfSchemaReaderWriter::GetInstance() ;
    confschemawrtrptr->ActOnCorruptedGroups() ;
}

INM_ERROR PairCreationHandler::ReconstructRepo(const std::string& agentGuid)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_Configurator = NULL;
	std::string cacheFile = getConfigCachePath(ConfSchemaMgr::GetInstance()->getRepoLocation(), agentGuid) ;
	DebugPrintf(SV_LOG_DEBUG, "cache file path %s\n", cacheFile.c_str()) ;
	Configurator::instanceFlag = false ;
	if (boost::filesystem::exists(cacheFile) ) 
	{
		if(!::InitializeConfigurator(&m_Configurator, 
			USE_ONLY_CACHE_SETTINGS, 
			Xmlize, cacheFile))
		{
			throw "Could not initialize the configurator" ;
		}
	}
	LoadAllConfigurationFiles() ;
	if( DeleteCorruptedConfigFiles() )
	{
		if ( m_Configurator != NULL )
		{
			ReconstrProtectionPairConfig( ) ;
		}
		ReconstructPolicyConfig() ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return E_SUCCESS; 
}