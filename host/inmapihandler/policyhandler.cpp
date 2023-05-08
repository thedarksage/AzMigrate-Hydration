#ifdef SV_WINDOWS
#include <winsock2.h>
#endif
#include "applicationsettings.h"
#include "policyhandler.h"
#include "apinames.h"
#include "inmageex.h"
#include "util.h"
#include "xmlmarshal.h"
#include "serializeapplicationsettings.h"
#include "inmsdkexception.h"
#include "inmstrcmp.h"
#include "confengine/volumeconfig.h"
#include "confengine/policyconfig.h"
#include "confengine/scenarioconfig.h"
#include "confengine/alertconfig.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/eventqueueconfig.h"

PolicyHandler::PolicyHandler(void)
{
}
PolicyHandler::~PolicyHandler(void)
{
}

INM_ERROR PolicyHandler::process(FunctionInfo& f)
{
	Handler::process(f);
	INM_ERROR errCode = E_SUCCESS;

	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, POLICY_UPDATE) == 0)
	{
		try
        {
		errCode = PolicyUpdate(f);
		}
        catch (ContextualException& e)
        {
            DebugPrintf(SV_LOG_ERROR, "Caught Exception:%s \n",e.what());
            errCode = E_NO_DATA_FOUND;
        }
        catch( const boost::bad_lexical_cast & e)
        {
            DebugPrintf(SV_LOG_ERROR,"Caught Exception: bad cast type %s",e.what());
            errCode = E_NO_DATA_FOUND;
        }
	}
	return Handler::updateErrorCode(errCode);
}


INM_ERROR PolicyHandler::validate(FunctionInfo& f)
{
	INM_ERROR errCode = Handler::validate(f);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write handler specific validation here

	return errCode;
}


INM_ERROR PolicyHandler::PolicyUpdate(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_UNKNOWN;
    const char * const INDEX[] = {"1", "2", "3", "4", "5" };
    ConstParametersIter_t paramIter ;
    POLICYUPDATE_ENUM policyUpdateStatus = APP_POLICY_UPDATE_DELETED;
    paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]);
    SV_UINT policyId = boost::lexical_cast<SV_UINT>(paramIter->second.value) ;
    
    paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]);
    SV_LONGLONG instanceId = boost::lexical_cast<SV_LONGLONG>(paramIter->second.value) ;

    std::string status = f.m_RequestPgs.m_Params.find(INDEX[2])->second.value;

    std::string log  = f.m_RequestPgs.m_Params.find(INDEX[3])->second.value;
    
    std::string errorCodeDesc  = f.m_RequestPgs.m_Params.find(INDEX[4])->second.value;

    PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
    std::string policyType ;
    policyConfigPtr->GetPolicyType( policyId, policyType ) ;
    if( !policyType.empty() )
    {
        policyUpdateStatus = policyConfigPtr->UpdatePolicy(policyId, instanceId, status, log) ;
        if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Failed") == 0 )
		{
			ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
			std::string statusErrorMsg ;
			std::string planName ;
            scenarioConfigPtr->GetPlanName(planName) ;
            if( policyType != "VolumeProvisioningV2" )
			{
				statusErrorMsg = status + " with \"" ;
				statusErrorMsg += errorCodeDesc  + "\"" ;
			}
			else
			{
				std::string srcVolumename ;
				policyConfigPtr->GetSourceVolume( policyId, srcVolumename ) ;
				statusErrorMsg = status + " for volume \"" ;
				statusErrorMsg += srcVolumename  + "\"" ;
			}
			SendPolicyAlert(policyType, policyId, instanceId, planName, statusErrorMsg) ;
        }
        if( policyType.compare(VOL_PACK_PROVISON) == 0 ||
            policyType.compare(VOL_PACK_PROVISONV2) == 0)
        {
            VOLPACK_ACTION volpackAction ;
            volpackAction = policyConfigPtr->GetVolPackAction(policyId) ;
            VolPackUpdate volpackUpdate ;
            if( status.compare("Success") == 0 && volpackAction == VOLPACK_CREATE)
            {
                volpackUpdate = unmarshal<VolPackUpdate>(errorCodeDesc);
            }
			std::string srcVolName ;
			policyConfigPtr->GetSourceVolume( policyId, srcVolName) ;
            UpdateScenarioByVolumeProvisionResult(policyId,srcVolName, status, volpackAction, volpackUpdate) ;
        }
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

        ConfSchemaReaderWriterPtr readerWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        readerWriterPtr->Sync() ;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( policyUpdateStatus ), "1" );
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

void PolicyHandler::SendPolicyAlert( const std::string& policyType,SV_UINT policyId, 
                                     SV_LONGLONG instanceId, const std::string& planName, 
                                     const std::string& status )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
	std::string alertMsg ;
	if (policyType == "VolumeProvisioningV2" )
	{
		alertMsg = "Prepare backup media ";
	}
	else 
	{
		alertMsg = policyType + " job ";
	}
	alertMsg += status ;
	alertMsg += " for " ;
	alertMsg += planName ;
	alertMsg += " Plan";
	alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_ERROR, SV_ALERT_MODULE_RESYNC, alertMsg) ; // Modified ALERT to WARNING
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void PolicyHandler::UpdateScenarioByVolumeProvisionResult(SV_UINT policyID, 
														  const std::string& srcVolume,
														  const std::string & policyStaus,
                                                          VOLPACK_ACTION volpackAction,
                                                          const VolPackUpdate& volpackUpdate)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string scenarioStatus;
    if(volpackAction == VOLPACK_CREATE)
    {
        if(policyStaus == UPDATE_POLICY_INPROGRESS)
        {
            scenarioStatus = VOL_PACK_INPROGRESS;
        }
        else if(policyStaus == UPDATE_POLICY_FAILED)
        {
            scenarioStatus = VOL_PACK_FAILED;
        }
        else if(policyStaus == UPDATE_POLICY_SUCCESS)
        {
            scenarioStatus = VOL_PACK_SUCCESS;
        }
    }
    else if(volpackAction == VOLPACK_DELETE)
    {
        if(policyStaus == UPDATE_POLICY_INPROGRESS)
        {
            scenarioStatus = VOL_PACK_DELETION_INPROGRESS;
        }
        else if(policyStaus == UPDATE_POLICY_FAILED)
        {
            scenarioStatus = VOL_PACK_DELETION_FAILED;
        }
        else if(policyStaus == UPDATE_POLICY_SUCCESS)
        {
            scenarioStatus = VOL_PACK_DELETION_SUCCESS;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	if( volpackAction == VOLPACK_CREATE )
	{
	    std::map<std::string, std::string>::const_iterator volpackMountInfoIter ;
	    volpackMountInfoIter = volpackUpdate.m_volPackMountInfo.begin() ;
		if( policyStaus == "Success" )
		{
			scenarioConfigPtr->UpdateVolumeProvisioningFinalStatus(volpackMountInfoIter->first,
				volpackMountInfoIter->second) ;
	
		}
		else if( policyStaus == "Failed" )
		{
			scenarioConfigPtr->UpdateVolumeProvisioningFailureStatus(srcVolume) ;
		}
    }
    std::string existingStatus ;
    scenarioConfigPtr->GetExecutionStatus(existingStatus) ;
    if( existingStatus != "Active" )
    {
        scenarioConfigPtr->SetExecutionStatus(scenarioStatus) ;
    }
    if( VOL_PACK_DELETION_SUCCESS == scenarioStatus )
    {
		bool bRet = false;
		std::list <std::string> volumeListForDeletion;
 		ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
		PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
		std::list <std::string> targetVolumes = policyConfigPtr->GetVolumesInVolumeUnProvisioningPolicyData (policyID); 
		if ( protectionPairConfigPtr->GetSourceListFromTargetList (targetVolumes,volumeListForDeletion) ) 
		{
			protectionPairConfigPtr->ClearPairs(volumeListForDeletion);
			if (scenarioConfigPtr->removePairs (volumeListForDeletion) )
			{
				if (scenarioConfigPtr->removevolumesFromRepo (volumeListForDeletion))
				{
					DebugPrintf (SV_LOG_DEBUG ,"scenarioConfigPtr->removevolumesFromRepo success \n ");
					bRet = true;
				}
			}
		}
		if (bRet )
		{
			std::list<std::string> protectedVolumes ;
			VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance(); 
			scenarioConfigPtr->GetProtectedVolumes(protectedVolumes) ;
			//this is not required.. But in case we have some stale pairs left in scenarioconfig 
			//checking the protected volumes in protectioction config would make the plan to be cleared.
			if( protectedVolumes.size() != 0 )
			{
				protectedVolumes.clear() ;
				protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
			}
			if ( protectedVolumes.size() == 0) 
			{
				policyConfigPtr = PolicyConfig::GetInstance() ;
				scenarioConfigPtr->ClearPlan() ;
				policyConfigPtr->ClearPolicies() ;
			}
			else 
			{
				if( policyConfigPtr->isConsistencyPolicyEnabled() )
				{
					ConsistencyOptions consitencyOpts ;
					scenarioConfigPtr->GetConsistencyOptions( consitencyOpts )  ;
					std::list<std::string> protectedVolumes ;
					protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
					policyConfigPtr->UpdateConsistencyPolicy( consitencyOpts, protectedVolumes ) ;   
				}
			}
			volumeConfigPtr->RemoveVolumeResizeHistory (volumeListForDeletion); 
		}
		else 
		{
			DebugPrintf (SV_LOG_ERROR,"Delete backup protection fails \n ");
		}		
	}
}