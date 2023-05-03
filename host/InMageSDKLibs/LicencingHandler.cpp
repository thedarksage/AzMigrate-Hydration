#include "LicencingHandler.h"
#include "inmageex.h"
#include "inmstrcmp.h"
#include "inmsdkexception.h"
#include "ProtectionHandler.h"
#include "retlogpolicyobject.h"
#include "VolumeHandler.h"
#include "localconfigurator.h"
#include "confengine/scenarioconfig.h"
#include "confengine/confschemareaderwriter.h"
#include "util.h" 
#include "confengine/policyconfig.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/volumeconfig.h"
#include "ProtectionHandler.h"
#include "inmsdkutil.h"
#include "confengine/eventqueueconfig.h"
#include "util.h"
LicencingHandler::LicencingHandler(void)
{
}

LicencingHandler::~LicencingHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
INM_ERROR LicencingHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	INM_ERROR errCode = E_FUNCTION_NOT_EXECUTED;
	int systemPaused;
	if ( !GetSystemPausedState(systemPaused) )
		systemPaused = 0;

	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"UpdateProtectionPolicies") == 0 && systemPaused != 1)
	{
		errCode = UpdateProtectionPolicies(request);
	}
    else if( InmStrCmp<NoCaseCharCmp>("GetNodeLockString",m_handler.functionName.c_str()) == 0 )
    {
        errCode = GetNodeLockString(request) ;
    }
	else if( InmStrCmp<NoCaseCharCmp>("OnLicenseExpiry",m_handler.functionName.c_str()) == 0 )
	{
        errCode = OnLicenseExpiry(request) ;
	}
	else if( InmStrCmp<NoCaseCharCmp>("OnLicenseExtension",m_handler.functionName.c_str()) == 0 )
	{
		errCode = OnLicenseExtension(request) ;
	}
	else if( InmStrCmp<NoCaseCharCmp>("IsLicenseExpired",m_handler.functionName.c_str()) == 0 )
	{
		errCode = IsLicenseExpired(request) ;
	}
	return Handler::updateErrorCode(errCode);
}

INM_ERROR LicencingHandler::validate(FunctionInfo& request)
{
	return Handler::validate(request);	
}

INM_ERROR LicencingHandler::GetNodeLockString(FunctionInfo& request)
{
    INM_ERROR errCode = E_SUCCESS ;
    std::string biosId ;
    GetBiosId(biosId) ;
    ValueType vt ;
    vt.value = biosId ;
    request.m_ResponsePgs.m_Params.insert( std::make_pair("NodeLockString", vt) ) ;
    return errCode ;
}
INM_ERROR LicencingHandler::OnLicenseExpiry(FunctionInfo& request)
{
    INM_ERROR errCode = E_SUCCESS ;
    ProtectionHandler ph ;
    ProtectionPairConfigPtr protectionpairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ValueType vt ;
	vt.value = "License Expired" ;
	request.m_RequestPgs.m_Params.insert(std::make_pair( "Pause Reason",  vt)) ;
    ph.PauseProtection(request) ;
    return E_SUCCESS ;
}
INM_ERROR LicencingHandler::OnLicenseExtension(FunctionInfo& request)
{
    INM_ERROR errCode = E_SUCCESS ;
    ProtectionHandler ph ;
    ProtectionPairConfigPtr protectionpairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::list<std::string> protectedVolumes ;
    protectionpairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
    ph.ResumeProtection(request) ;
    return E_SUCCESS ;
}
bool GetIOParams(FunctionInfo& request, spaceCheckParameters& spacecheckparams)
{
	spacecheckparams.changeRate = 0.03 ;
	spacecheckparams.compressionBenifits = 0.4 ;
	if( request.m_RequestPgs.m_ParamGroups.find("IOParameters") != request.m_RequestPgs.m_ParamGroups.end() )
	{
		spacecheckparams.changeRate = boost::lexical_cast<double>(request.m_RequestPgs.m_ParamGroups.find("IOParameters")->second.m_Params.find( "ChangeRate" )->second.value); 
		spacecheckparams.compressionBenifits = boost::lexical_cast<double>(request.m_RequestPgs.m_ParamGroups.find("IOParameters")->second.m_Params.find( "CompressionBenefits" )->second.value); 
	}
	return true ;
}
INM_ERROR LicencingHandler::UpdateProtectionPolicies(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS;
    LocalConfigurator configurator ;
	std::string  hostID;
	std::string repoName, repoPath;
	SV_ULONGLONG repoCapacity  = 0 ,requiredSpace = 0,repoFreeSpace = 0; 
	hostID = configurator.getHostId() ;
	bool  bSpaceAvailabililty = false ;
	ScenarioConfigPtr scenarioConf = ScenarioConfig::GetInstance();
	ProtectionPairConfigPtr pairConf = ProtectionPairConfig::GetInstance();
	VolumeConfigPtr volumeConf = VolumeConfig::GetInstance();
	PolicyConfigPtr policyConfig = PolicyConfig::GetInstance() ;
	std::list<std::string> protectedVolumes ;
	std::map<std::string, volumeProperties> volumePropertiesMap ;
	std::string recoveryFeatureFromRequest, rpoPolicyFromRequest;
	ConsistencyOptions consistencyOptionsFromRequest ;
	RetentionPolicy retentionPolicyFromRequest;
	ReplicationOptions replicationOptionsFromRequest ;
	SV_UINT rpoThreshold;
	ConfSchemaReaderWriterPtr readerWriter = ConfSchemaReaderWriter::GetInstance() ;
    if( scenarioConf->ProtectionScenarioAvailable() )
	{
		bool bRPOPolicy = GetRPOPolicy( request, rpoPolicyFromRequest ) ;
		if( bRPOPolicy != true )
		{
			scenarioConf->GetRpoThreshold(rpoThreshold);
		}
		else 
			rpoThreshold = boost::lexical_cast <SV_UINT> (rpoPolicyFromRequest) ;

		bool bRecoveryFeature = GetRecoveryFeature( request, recoveryFeatureFromRequest ) ;
		bool bConsistencyPolicy = GetConsistencyOptions( request, consistencyOptionsFromRequest ) ;
		bool bRetentionPolicy = GetRetentionPolicyFromRequest( request, retentionPolicyFromRequest ) ;
		SV_INT oldbatchSize = scenarioConf->GetBatchSize();
		bool bReplicationOptions = GetReplicationOptionsForUpdateRequest(request, replicationOptionsFromRequest) ;
		spaceCheckParameters spacecheckparams, spacecheckparams_original ;
		bool bIOParams = GetIOParams(request, spacecheckparams) ;
		scenarioConf -> GetIOParameters(spacecheckparams_original) ;
		if(spacecheckparams_original.changeRate != spacecheckparams.changeRate || spacecheckparams_original.compressionBenifits != spacecheckparams.compressionBenifits )
			bIOParams = true ;
		else
			bIOParams = false ;
		volumeConf->GetRepositoryNameAndPath( repoName, repoPath ) ;
		volumeConf->GetRepositoryCapacity (repoName, repoCapacity) ;
		volumeConf->GetRepositoryFreeSpace (repoName, repoFreeSpace);
		volumeConf->GetVolumeProperties(volumePropertiesMap) ;
		scenarioConf->GetProtectedVolumes(protectedVolumes) ;
		if ( bRetentionPolicy || bIOParams )
		{
			std::map<std::string, PairDetails> pairDetailsMap ;
			GetPairDetails(rpoThreshold, retentionPolicyFromRequest, repoName, hostID, protectedVolumes, volumePropertiesMap, pairDetailsMap) ;
			SV_ULONGLONG protectedVolumeOverHead = 0;
			std::map <std::string,hostDetails> hostDetailsMap; 
			double spaceMultiplicationFactor = 0 ; // GetSpaceMultiplicationFactorFromRequest (request);
			std::string overRideSpaceCheck ; 
			isSpaceCheckRequired (request , overRideSpaceCheck);
			if (overRideSpaceCheck !=  "Yes")
			{
				double compressionBenifitsAfterUpdate = spacecheckparams.compressionBenifits; 
				double compressionBenifitsBeforeUpdate = spacecheckparams_original.compressionBenifits; 
				double sparseSavingsFactor = 3.0; 
				double changeRateAfterUpdate = spacecheckparams.changeRate ;
				double changeRateBeforeUpdate = spacecheckparams_original.changeRate ;
				double ntfsMetaDataoverHead = 0.13;  
				RetentionPolicy currentRetentionPolicy ; 
				scenarioConf->GetRetentionPolicy( "", currentRetentionPolicy )  ;
				SV_ULONGLONG currentRequiredSpace = 0,requiredSpaceAfterUpdatePolicy = 0,repoFreeSpace1;

				/*bSpaceAvailabililty =  SpaceCalculationV2 (	repoPath,
															protectedVolumes , 
															retentionPolicyFromRequest ,
															requiredSpace , 
															protectedVolumeOverHead,
															repoFreeSpace,hostDetailsMap,
															compressionBenifits,
															sparseSavingsFactor,
															changeRate,
															ntfsMetaDataoverHead);*/
				CheckSpaceAvailability (repoPath,
										currentRetentionPolicy ,
										protectedVolumes,
										currentRequiredSpace,
										repoFreeSpace1,
										compressionBenifitsBeforeUpdate , 
										sparseSavingsFactor,
										changeRateBeforeUpdate,
										ntfsMetaDataoverHead  );
				CheckSpaceAvailability (repoPath,
										retentionPolicyFromRequest ,
										protectedVolumes,
										requiredSpaceAfterUpdatePolicy,
										repoFreeSpace1,
										compressionBenifitsAfterUpdate, 
										sparseSavingsFactor,
										changeRateAfterUpdate,
										ntfsMetaDataoverHead  );
				SV_ULONGLONG extraRequiredSpace = 0 ;
				if ( requiredSpaceAfterUpdatePolicy > currentRequiredSpace )
				{
					extraRequiredSpace = requiredSpaceAfterUpdatePolicy - currentRequiredSpace ;
				}
				if (extraRequiredSpace > repoFreeSpace1 )
				{
					bSpaceAvailabililty = false ;

				}
				else
				{
					bSpaceAvailabililty = true ;

				}
				if ( !bSpaceAvailabililty )
				{
					errCode = E_REPO_VOL_HAS_INSUCCICIENT_STORAGE ;
					/*request.m_Message = FormatSpaceCheckMsg(request.m_RequestFunctionName,
																hostDetailsMap ) ;*/
					std::string msg ;
					msg += ". Additional  " ;
					msg += convertToHumanReadableFormat (extraRequiredSpace ) ;
					msg += " free space is required, but only " ;
					msg += convertToHumanReadableFormat (repoFreeSpace1) ;
					msg += " is available. " ;
					msg += "Please choose a different backup location with atleast " ;
					msg += convertToHumanReadableFormat (requiredSpaceAfterUpdatePolicy) ;
					msg += " free. " ;
					msg += "You can also try reducing the number of days that backup data is retained." ;
					//request.m_Message = FormatSpaceCheckMsg(request.m_RequestFunctionName, hostDetailsMap) ;
					request.m_Message = msg ;
				}
			}
			else 
			{
				bSpaceAvailabililty = true; 
			}
		}
		else
		{
			bSpaceAvailabililty = true ;
		}
		policyConfig = PolicyConfig::GetInstance() ;
		pairConf = ProtectionPairConfig::GetInstance();
		scenarioConf = ScenarioConfig::GetInstance();		
		if (bSpaceAvailabililty)
		{
			if( bRecoveryFeature == true )
			{
				scenarioConf->PopulateRecoveryFeaturePolicy(recoveryFeatureFromRequest)  ;
			}
	
			if( bConsistencyPolicy == true ) 
			{
				scenarioConf->PopulateConsistencyOptions( consistencyOptionsFromRequest ) ;            
				policyConfig->UpdateConsistencyPolicy( consistencyOptionsFromRequest,  protectedVolumes ) ;
			}
			scenarioConf->UpdatePairObjects( rpoPolicyFromRequest, retentionPolicyFromRequest,  bRPOPolicy,  bRetentionPolicy) ;                
			if( bRetentionPolicy )
				pairConf->UpdatePairsWithRetention( retentionPolicyFromRequest ) ;        

			if ( bReplicationOptions )
			{
				scenarioConf->PopulateReplicationOptions (replicationOptionsFromRequest);
				pairConf->ActOnQueuedPair();
			}
			if( bIOParams )
			{
				scenarioConf->UpdateIOParameters(spacecheckparams);
			}
			EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
			eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;
			ConfSchemaReaderWriter::GetInstance()->Sync() ;
		}
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

	return errCode;
}

INM_ERROR LicencingHandler::IsLicenseExpired(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance();
	bool ret = false ;
	int pausedState ;
	GetSystemPausedState(pausedState);
	std::string pausereason = scenarioConfigPtr->GetSystemPausedReason() ;
	if( pausedState == 1 && pausereason ==  "License Expired" )
	{
		ret = true ;
	}
	insertintoreqresp(request.m_ResponsePgs, cxArg ( ret ), "1");
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return E_SUCCESS ;
}
