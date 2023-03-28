#include "RecoveryHandler.h"
#include "inmstrcmp.h"
#include "inmsdkexception.h"
#include "inmageex.h"
#include "VolumeHandler.h"
#include "MonitorHandler.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/scenarioconfig.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/volumeconfig.h"
#include "util.h"
#include "APIController.h"
RecoveryHandler::RecoveryHandler(void)
{
}
RecoveryHandler::~RecoveryHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
INM_ERROR RecoveryHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	INM_ERROR errCode = E_SUCCESS;
	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListRestorePoints") == 0)
	{
		errCode = ListRestorePoints(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"RecoverVolumesToLCCP") == 0)
	{
		//errCode = RecoverVolumesToLCCP(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"RecoverVolumeToLCT") == 0)
	{
		//errCode = RecoverVolumeToLCT(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"RecoverVolumeToRestorePoint") == 0)
	{
		//errCode = RecoverVolumeToRestorePoint(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"SnapshotVolumeToLCP") == 0)
	{
		//errCode = SnapshotVolumeToLCP(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"SnapshotVolumeTorestorePoint") == 0)
	{
		//errCode = SnapshotVolumeTorestorePoint(request);
	}
	else
	{
		//Through an exception that, the function is not supported.
	}
	return Handler::updateErrorCode(errCode);
}
INM_ERROR RecoveryHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}

INM_ERROR RecoveryHandler::ListRestorePoints(FunctionInfo& request)
{
    INM_ERROR errCode = E_SUCCESS ;
	LocalConfigurator  lc ;
	std::string actualbasepath, newbasepath, agentguid ;
	if( request.m_RequestPgs.m_Params.find( "RepositoryPath" ) != request.m_RequestPgs.m_Params.end() )
	{
		newbasepath = request.m_RequestPgs.m_Params.find( "RepositoryPath" )->second.value ;
	}
	else
	{
		try
		{
			newbasepath = lc.getRepositoryLocation() ;
		}
		catch(ContextualException& ex)
		{
			return E_NO_REPO_CONFIGERED ;
		}
	}
	if( request.m_RequestPgs.m_Params.find( "AgentGUID" ) != request.m_RequestPgs.m_Params.end() )
	{
		agentguid = request.m_RequestPgs.m_Params.find( "AgentGUID" )->second.value;
	}
	else
	{
		agentguid = lc.getHostId() ;
	}
	std::string repoPathForHost = constructConfigStoreLocation( getRepositoryPath(newbasepath),  agentguid) ;
	std::string lockPath = getLockPath(repoPathForHost) ;
	RepositoryLock repoLock(lockPath) ;
	repoLock.Acquire() ;
	ConfSchemaReaderWriter::CreateInstance( request.m_RequestFunctionName, getRepositoryPath(lc.getRepositoryLocation()),  repoPathForHost, false) ;
	std::string repositoryname ;
    ScenarioConfigPtr scenarionConfigPtr = ScenarioConfig::GetInstance() ;
	SV_UINT rpoThreshold ;
    scenarionConfigPtr->GetRpoThreshold(rpoThreshold) ;
    ProtectionPairConfigPtr protectionPairConfig = ProtectionPairConfig::GetInstance() ;
	std::list<std::string> protectedvolumes ;
	std::map<std::string, std::string> srcVolRetentionPathMap ;
	protectionPairConfig->GetRetentionPath(srcVolRetentionPathMap) ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	volumeConfigPtr->GetRepositoryNameAndPath(repositoryname, actualbasepath) ;
	repoLock.Release() ;
	SV_UINT restorePointCount = 0 ;
	Parameters_t::const_iterator paramIter 	= request.m_RequestPgs.m_Params.find("RestorePointCount");
	if(paramIter!=request.m_RequestPgs.m_Params.end())
	{
		std::string restorePointCountStr = paramIter->second.value;
		restorePointCount = boost::lexical_cast<SV_UINT>( restorePointCountStr ) ;
	}

    bool bRestorePointsAvailability = false ;
    bool bNoCommonRestorePoints = false ;
	if( !srcVolRetentionPathMap.empty() )
    {
        std::map<std::string, std::map<SV_ULONGLONG, CDPEvent> > volumeRPMap ;
        std::map<std::string, std::map<SV_ULONGLONG, CDPTimeRange> > volumeRRMap ;
		std::map<std::string, std::string>::const_iterator srcvolretentionpathIter = srcVolRetentionPathMap.begin() ;

        while( srcvolretentionpathIter != srcVolRetentionPathMap.end() )
        {
            ParameterGroup restorePointsPg, recoveryRangesPg, recoveryRestoreGroups  ;
            std::map<SV_ULONGLONG, CDPEvent>  volumeRestorePoints ;
            std::map<SV_ULONGLONG, CDPTimeRange> volumeRecoveryRanges ;
                         
            GetAvailableRestorePointsPg( actualbasepath, newbasepath, srcvolretentionpathIter->second, rpoThreshold, restorePointsPg, volumeRestorePoints, restorePointCount ) ;
            GetAvailableRecoveryRangesPg(  actualbasepath, newbasepath, srcvolretentionpathIter->second, rpoThreshold, recoveryRangesPg, volumeRecoveryRanges, restorePointCount ) ;
            if( volumeRestorePoints.empty() && volumeRecoveryRanges.empty() )
                bNoCommonRestorePoints = true ;
            volumeRPMap.insert( std::make_pair( srcvolretentionpathIter->first, volumeRestorePoints ) ) ;
            volumeRRMap.insert( std::make_pair( srcvolretentionpathIter->first, volumeRecoveryRanges ) ) ;
            ValueType vt;
			vt.value = srcvolretentionpathIter->first ;
			
            if( bRestorePointsAvailability == false && 
                ( volumeRestorePoints.empty() == false || volumeRecoveryRanges.empty() == false )  )
                bRestorePointsAvailability = true ;

            recoveryRestoreGroups.m_Params.insert(std::make_pair("VolumeName",vt));
            recoveryRestoreGroups.m_ParamGroups.insert(std::make_pair("AvailableRestorePoints",restorePointsPg));
            recoveryRestoreGroups.m_ParamGroups.insert(std::make_pair("AvailableRecoveryRanges",recoveryRangesPg));
            request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(srcvolretentionpathIter->first,recoveryRestoreGroups));
            srcvolretentionpathIter++ ;
        }
        if( bRestorePointsAvailability == false ) 
            errCode = E_NO_RESTORE_POINT ;
        else if( bNoCommonRestorePoints == false )
            FillCommonRestorePointInfo(request.m_ResponsePgs, volumeRPMap, volumeRRMap) ;
    }
    else
    {
        errCode = E_NO_PROTECTIONS ;
    }
    return errCode ;
}
