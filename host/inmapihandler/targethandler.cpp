#include "targethandler.h"
#include "inmstrcmp.h"
#include "apinames.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/alertconfig.h"
#include "confengine/volumeconfig.h"
#include "xmlizeretentioninformation.h"
#include "confengine/eventqueueconfig.h"

TargetHandler::TargetHandler(void)
{
}

TargetHandler::~TargetHandler(void)
{
}

INM_ERROR TargetHandler::process(FunctionInfo& f)
{
	Handler::process(f);
	INM_ERROR errCode = E_SUCCESS;

	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, NOTIFY_CX_DIFFS_DRAINED) == 0)
	{
		errCode = notifyCxDiffsDrained(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, CDP_STOP_REPLICATION) == 0)
	{
		errCode = cdpStopReplication(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SEND_END_QUASI_STATE_REQUEST) == 0)
	{
		errCode = sendEndQuasiStateRequest(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SET_TARGET_RESYNC_REQUIRED) == 0)
	{
		errCode = setTargetResyncRequired(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_VOLUME_ATTRIBUTE) == 0)
	{
		errCode = updateVolumeAttribute(f);
	}
    
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_CURRENT_VOLUME_ATTRIBUTE) == 0)
	{
		errCode = getCurrentVolumeAttribute(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_CDP_INFORMATIONV2) == 0)
	{
		errCode = updateCDPInformationV2(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_CDP_DISKUSAGE) == 0 )
    {
        errCode = updateCdpDiskUsage(f);
    }
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_HOST_RETENTION_WINDOW) == 0)
	{
		errCode = getHostRetentionWindow(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_REPLICATION_CLEANUP_STATUS) == 0)
	{
		errCode = updateCleanUpActionStatus(f);
	} 
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_RETENTION_INFO) == 0)
	{
		errCode = updateRetentionInfo(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SEND_ALERT_TO_CX) == 0)
	{
		errCode = sendAlertToCx(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SET_PAUSE_REPLICATION_STATUS) == 0)
	{
		errCode = setPauseReplicationStatus(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_FLUSH_AND_HOLD_WRITES_PENDING) == 0)
	{
		errCode = updateFlushAndHoldWritesPendingStatus(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_FLUSH_AND_HOLD_RESUME_PENDING) == 0)
	{
		errCode = updateFlushAndHoldResumePendingStatus(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_FLUSH_AND_HOLD_WRITES_REQUEST_SETTINGS) == 0)
	{
		errCode = getFlushAndHoldRequestSettings(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, PAUSE_REPLICATION_FROM_HOST) == 0)
	{
		errCode = pauseReplicationFromHost(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, RESUME_REPLICATION_FROM_HOST) == 0)
	{
		errCode = resumeReplicationFromHost(f);
	}

	else
	{
		//Through an exception that, the function is not supported.
	}
	return Handler::updateErrorCode(errCode);
}

INM_ERROR TargetHandler::resumeReplicationFromHost(FunctionInfo& f)
{
	std::string tgtdevname = f.m_RequestPgs.m_Params.find("2")->second.value ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	protectionPairConfigPtr->ResumeProtectionByTgtVolume(tgtdevname) ;
	ConfSchemaReaderWriter::GetInstance()->Sync() ;
	insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    return E_SUCCESS ;
}

INM_ERROR TargetHandler::pauseReplicationFromHost(FunctionInfo& f)
{
	std::string tgtdevname = f.m_RequestPgs.m_Params.find("2")->second.value ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	protectionPairConfigPtr->PauseProtectionByTgtVolume(tgtdevname) ;
	ConfSchemaReaderWriter::GetInstance()->Sync() ;
	insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    return E_SUCCESS ;
}
INM_ERROR TargetHandler::validate(FunctionInfo& f)
{
	INM_ERROR errCode = Handler::validate(f);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write handler specific validation here

	return errCode;
}
INM_ERROR TargetHandler::updateCdpDiskUsage(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_SUCCESS;
    const char * const INDEX[] = {"2", "3"};
    bool proceed = true ;
    ParameterGroup hostcdpretentiondiskusagePg, hostcdptargetusagePg ;
    ParameterGroupsIter_t pgIter = f.m_RequestPgs.m_ParamGroups.find( INDEX[0] ) ;
    if( pgIter != f.m_RequestPgs.m_ParamGroups.end() )
    {
        hostcdpretentiondiskusagePg = pgIter->second ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Couldn't find pg at index 2. %s\n", f.m_RequestFunctionName.c_str()) ;
        proceed = false ;
    }
    pgIter = f.m_RequestPgs.m_ParamGroups.find( INDEX[1] ) ;
    if( pgIter != f.m_RequestPgs.m_ParamGroups.end() )
    {
        hostcdptargetusagePg = pgIter->second ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Couldn't find pg at index 3. %s\n", f.m_RequestFunctionName.c_str()) ;
        proceed = false ;
    }
    if( proceed )
    {
        pgIter = hostcdpretentiondiskusagePg.m_ParamGroups.begin() ;
        while( pgIter != hostcdpretentiondiskusagePg.m_ParamGroups.end() )
        {
            const std::string& tgtVolume = pgIter->first ;
            ParameterGroup& cdpretentiondiskusagepg = pgIter->second ;
            UpdateRetentionDiskUsageInfo(tgtVolume, cdpretentiondiskusagepg ) ;
            pgIter++ ;
        }
        pgIter = hostcdptargetusagePg.m_ParamGroups.begin() ;
        while( pgIter != hostcdptargetusagePg.m_ParamGroups.end() )
        {
            const std::string& tgtVolume = pgIter->first ;
            ParameterGroup& cdptargetdiskusagePg = pgIter->second ;
            UpdateTargetDiskUsageInfo(tgtVolume, cdptargetdiskusagePg) ;
            pgIter++ ;
        }
    }
    ConfSchemaReaderWriterPtr confschemardrwrter = ConfSchemaReaderWriter::GetInstance() ;
    confschemardrwrter->Sync() ;
    insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}
void TargetHandler::UpdateTargetDiskUsageInfo(const std::string& tgtVolume,
                                              const ParameterGroup& cdptargetdiskusagePg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SV_ULONGLONG sizeOnDisk, spaceSavedByCompression, spaceSavedByThinProvisioning ;
    ConstParametersIter_t paramIter ;
    bool proceed = true ;
    do
    {
        const Parameters_t& params = cdptargetdiskusagePg.m_Params ;
        paramIter = params.find( NS_CDPTargetDiskUsage::size_on_disk) ;
        if( paramIter != params.end() )
        {
            sizeOnDisk = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdptargetdiskusagePg params\n", 
                        NS_CDPTargetDiskUsage::size_on_disk ) ;
            proceed = false ;
        }
        paramIter = params.find( NS_CDPTargetDiskUsage::space_saved_by_compression) ;
        if( paramIter != params.end() )
        {
            spaceSavedByCompression = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdptargetdiskusagePg params\n", 
                    NS_CDPTargetDiskUsage::space_saved_by_compression) ;
            proceed = false ;
        }
        paramIter = params.find( NS_CDPTargetDiskUsage::space_saved_by_thinprovisioning) ;
        if( paramIter != params.end() )
        {
            spaceSavedByThinProvisioning = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdptargetdiskusagePg params\n", 
                        NS_CDPTargetDiskUsage::space_saved_by_thinprovisioning) ;
            proceed = false ;
        }
        
    } while( false ) ;
    if( proceed )
    {
        VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
        volumeConfigPtr->UpdateTargetDiskUsageInfo( tgtVolume, sizeOnDisk, 
                                                    spaceSavedByCompression, 
                                                    spaceSavedByThinProvisioning ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void TargetHandler::UpdateRetentionDiskUsageInfo(const std::string& tgtVolume,
                                                 const ParameterGroup& cdpretentiondiskusagepg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SV_ULONGLONG endTs, startTs, sizeOnDisk, spaceSavedByCompression, spaceSavedBySparsePolicy ;
    SV_UINT numFiles ;
    bool proceed = true ;
    do
    {
        ConstParametersIter_t paramIter ;
        const Parameters_t& params = cdpretentiondiskusagepg.m_Params ;
        paramIter = params.find( NS_CDPRetentionDiskUsage::end_ts ) ;
        if( paramIter != params.end() )
        {
            endTs = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdpretentiondiskusagepg params\n", 
                        NS_CDPRetentionDiskUsage::end_ts ) ;
            proceed = false ;
        }
        paramIter = params.find( NS_CDPRetentionDiskUsage::num_files ) ;
        if( paramIter != params.end() )
        {
            numFiles = boost::lexical_cast<SV_UINT>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdpretentiondiskusagepg params\n", 
                            NS_CDPRetentionDiskUsage::num_files ) ;
            proceed = false ;

        }
        paramIter = params.find( NS_CDPRetentionDiskUsage::size_on_disk ) ;
        if( paramIter != params.end() )
        {
            sizeOnDisk = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdpretentiondiskusagepg params\n", 
                        NS_CDPRetentionDiskUsage::size_on_disk ) ;
            proceed = false ;
        }
        paramIter = params.find( NS_CDPRetentionDiskUsage::space_saved_by_compression ) ;
        if( paramIter != params.end() )
        {
            spaceSavedByCompression = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdpretentiondiskusagepg params\n", 
                        NS_CDPRetentionDiskUsage::space_saved_by_compression ) ;
            proceed = false ;
        }
        paramIter = params.find( NS_CDPRetentionDiskUsage::space_saved_by_sparsepolicy ) ;
        if( paramIter != params.end() )
        {
            spaceSavedBySparsePolicy = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdpretentiondiskusagepg params\n", 
                            NS_CDPRetentionDiskUsage::space_saved_by_sparsepolicy ) ;
            proceed = false ;
        }
        paramIter = params.find( NS_CDPRetentionDiskUsage::start_ts ) ;
        if( paramIter != params.end() )
        {
            startTs = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s not found  in cdpretentiondiskusagepg params\n", 
                            NS_CDPRetentionDiskUsage::start_ts ) ;
            proceed = false ;

        }
    } while( 0 ) ;
    if( proceed )
    {
        ProtectionPairConfigPtr protectionPairConfigPtr ;
        protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        protectionPairConfigPtr->UpdateCDPRetentionDiskUsage( tgtVolume, startTs, endTs, sizeOnDisk, 
                                    spaceSavedByCompression, spaceSavedBySparsePolicy, numFiles) ;

    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Cannot update the configstore with cdp retention disk usge information\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


INM_ERROR TargetHandler::notifyCxDiffsDrained(FunctionInfo &f)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_SUCCESS;
    //Return Success as this does not have any significance. This is an old API still being called for no purpose
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR TargetHandler::cdpStopReplication(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_FUNCTION_NOT_SUPPORTED;
    //Return Success as this does not have any significance. This is an old API still being called for no purpose
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR TargetHandler::sendEndQuasiStateRequest(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_UNKNOWN;
    const char * const INDEX[] = {"2"};
    bool proceed = true ;
    std::string tgtDevName ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    ConstParametersIter_t paramIter ;

    do
    {
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 2 param in the requestPgs params list\n" ) ;
            proceed = false ;
            break ;

        }
    } while( 0 ) ;
    if( proceed )
    {
        protectionPairConfigPtr->UpdateQuasiEndState(tgtDevName) ;
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

        ConfSchemaReaderWriterPtr confschemareaderWriterPtr ;
        confschemareaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        confschemareaderWriterPtr->Sync() ;
        errCode = E_SUCCESS;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;

}

INM_ERROR TargetHandler::setTargetResyncRequired(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_UNKNOWN;
    bool proceed = true ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    ConstParametersIter_t paramIter ;
    const char * const INDEX[] = {"2","3"};
    std::string tgtDevName, errMsg ;
    bool bReturnCode = false ;
    do
    {
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]);
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 2 param in the requestPgs params list\n" ) ;
            proceed = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]);
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            errMsg = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the requestPgs params list\n" ) ;
            proceed = false ;
            break ;
        }        
    } while( 0 ) ;
    if( proceed )
    {
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

        protectionPairConfigPtr->SetResyncRequiredByTarget(tgtDevName , errMsg ) ;
        ConfSchemaReaderWriterPtr confschemareaderWriterPtr ;
        confschemareaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        confschemareaderWriterPtr->Sync() ;
        errCode = E_SUCCESS;
        bReturnCode = true ;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( bReturnCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return errCode ;
}

INM_ERROR TargetHandler::updateVolumeAttribute(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_SUCCESS;
    //Return Success as this does not have any significance. This is an old API still being called for no purpose
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR TargetHandler::getCurrentVolumeAttribute(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_SUCCESS;
    //Return Success as this does not have any significance. This is an old API still being called for no purpose
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}


INM_ERROR TargetHandler::updateCDPInformationV2(FunctionInfo &f)
{
    const char * const INDEX[] = {"2"} ;
    ConstParameterGroupsIter_t cdpInfoMapIter   = f.m_RequestPgs.m_ParamGroups.find(INDEX[0]);
    bool proceed = false ;
    INM_ERROR errCode = E_UNKNOWN ;
	bool vxAgentErrorCode = false;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    if( cdpInfoMapIter != f.m_RequestPgs.m_ParamGroups.end() )
    {
        const ParameterGroup & cdpInfoMapPG = cdpInfoMapIter->second ;
        ConstParameterGroupsIter_t cdpInfoPGIter = cdpInfoMapPG.m_ParamGroups.begin();
        proceed = true ;
        while(cdpInfoPGIter != cdpInfoMapPG.m_ParamGroups.end())
        {
            const std::string & targetDeviceName = cdpInfoPGIter->first;
            const ParameterGroup &cdpInfo = cdpInfoPGIter->second;
            ConstParameterGroupsIter_t cdpSummartyPGIter = cdpInfo.m_ParamGroups.find(NS_CDPInformation::m_summary);
            const ParameterGroup &cdpSummaryPG = cdpSummartyPGIter->second ;
            protectionPairConfigPtr->UpdateCDPSummary(targetDeviceName, cdpSummaryPG) ;
            cdpInfoPGIter++ ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "didnt find the 2 pg in the requestPgs list\n" ) ;
    }
    if( proceed )
    {
        ConfSchemaReaderWriterPtr confschemareaderWriterPtr ;
        confschemareaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        confschemareaderWriterPtr->Sync() ;
        errCode = E_SUCCESS;
        vxAgentErrorCode = true;
    }
	
    insertintoreqresp(f.m_ResponsePgs, cxArg ( vxAgentErrorCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR TargetHandler::getHostRetentionWindow(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ParameterGroup pg;
    INM_ERROR errCode = E_UNKNOWN ;
    std::pair<ParameterGroupsIter_t, bool> retentionWindowPair ;
    retentionWindowPair = pg.m_ParamGroups.insert(std::make_pair(NS_RetentionInformation::m_window,ParameterGroup()));
    ParameterGroupsIter_t &retentionWindowIter = retentionWindowPair.first;
    ParameterGroup &retentionWindowPG = retentionWindowIter->second; 
    ProtectionPairConfigPtr protectionPairConfigPtr ;
    protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::list<std::string> targetVolumes ;
    protectionPairConfigPtr->GetProtectedTargetVolumes(targetVolumes) ;
    std::list<std::string>::const_iterator tgtVolIter = targetVolumes.begin() ;
    while( tgtVolIter != targetVolumes.end() )
    {
        std::string logsFrom, logsTo, logsFromUtc, logsToUtc ;
        if( protectionPairConfigPtr->GetRetentionWindowDetailsByTgtVolName(*tgtVolIter, logsFrom, logsTo, logsFromUtc, logsToUtc) )
        {
            errCode = E_SUCCESS ;
            retentionWindowPG.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, 
                                                    ValueType("unsigned int", logsFrom)));
            retentionWindowPG.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, 
                                                    ValueType("unsigned int", logsTo)));
            retentionWindowPG.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, 
                                                    ValueType("unsigned int", logsFromUtc)));
            retentionWindowPG.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, 
                                                    ValueType("unsigned int", logsToUtc)));
            pg.m_ParamGroups.insert(std::make_pair(*tgtVolIter,retentionWindowPG));
        }
        tgtVolIter++ ;
    }
    f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("1",pg));
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR TargetHandler::updateCleanUpActionStatus(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    INM_ERROR errCode = E_UNKNOWN ;
    const char * const INDEX[] = {"2","3"};
    std::string targetDevName, cleanupActionStr ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance();
    bool process = true ;
    do
    {
        ConstParametersIter_t paramIter ;
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]);
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            targetDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 2 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]);
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            cleanupActionStr = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }

    } while( 0 ) ;
    if( process )
    {
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;
        protectionPairConfigPtr->UpdateCleanUpAction(targetDevName, cleanupActionStr) ;
        ConfSchemaReaderWriterPtr confschemaReaderWriterPtr ;
        confschemaReaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        confschemaReaderWriterPtr->Sync() ;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;

}

INM_ERROR TargetHandler::updateRetentionInfo(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_FUNCTION_NOT_SUPPORTED;
    //Return Success as this does not have any significance. This is an old API still being called for no purpose
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}


INM_ERROR TargetHandler::sendAlertToCx(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_UNKNOWN;
    AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;


    const char * const INDEX[] = {"2","3","4","5","6","7"};
    if( f.m_RequestPgs.m_Params.find(INDEX[0]) != f.m_RequestPgs.m_Params.end() ||
        f.m_RequestPgs.m_Params.find(INDEX[1]) != f.m_RequestPgs.m_Params.end() ||
        f.m_RequestPgs.m_Params.find(INDEX[2]) != f.m_RequestPgs.m_Params.end() ||
        f.m_RequestPgs.m_Params.find(INDEX[3]) != f.m_RequestPgs.m_Params.end() ||
        f.m_RequestPgs.m_Params.find(INDEX[4]) != f.m_RequestPgs.m_Params.end() ||
        f.m_RequestPgs.m_Params.find(INDEX[5]) != f.m_RequestPgs.m_Params.end() )
    {
        errCode = E_SUCCESS ;
		std::string alertPrefix;
        std::string timeStr  = f.m_RequestPgs.m_Params.find(INDEX[0])->second.value ;
        std::string errLevel  = f.m_RequestPgs.m_Params.find(INDEX[1])->second.value ;
        std::string agentType  = f.m_RequestPgs.m_Params.find(INDEX[2])->second.value ;
        std::string alertMsg = f.m_RequestPgs.m_Params.find(INDEX[3])->second.value ;
        SV_ALERT_MODULE alertingModule  = (SV_ALERT_MODULE) boost::lexical_cast<int>(f.m_RequestPgs.m_Params.find(INDEX[4])->second.value);
        SV_ALERT_TYPE alertType  = (SV_ALERT_TYPE) boost::lexical_cast<int>(f.m_RequestPgs.m_Params.find(INDEX[5])->second.value) ;

		alertConfigPtr->processAlert (alertMsg,alertPrefix);
        alertConfigPtr->AddAlert(timeStr, errLevel, agentType, alertType, alertingModule, alertMsg, alertPrefix) ;
    }
    if( errCode == E_SUCCESS )
    {
        ConfSchemaReaderWriterPtr confSchemaReaderWriterPtr ;
        confSchemaReaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        confSchemaReaderWriterPtr->Sync() ;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}


INM_ERROR TargetHandler::setPauseReplicationStatus(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_UNKNOWN;
    const char * const INDEX[] = {"2","3","4"} ;
    std::string devName, actionStr ;
    bool process = true ;
    ProtectionPairConfigPtr protectionPairConfigPtr  = ProtectionPairConfig::GetInstance();
    int hostType ;
    ConstParametersIter_t paramiter ;
    do
    {
        paramiter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramiter != f.m_RequestPgs.m_Params.end() )
        {
            devName = paramiter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 2 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramiter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramiter != f.m_RequestPgs.m_Params.end() )
        {
            hostType = boost::lexical_cast<int>(paramiter->second.value) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 2 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramiter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramiter != f.m_RequestPgs.m_Params.end() )
        {
            actionStr = paramiter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 4 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    } while( 0 ) ;
    if( process )
    {
        errCode = E_SUCCESS ;
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

        protectionPairConfigPtr->UpdatePauseReplicationStatus(devName, hostType, actionStr) ;
        ConfSchemaReaderWriterPtr confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance();
        confschemareaderwriterPtr->Sync() ;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;

}

INM_ERROR TargetHandler::updateFlushAndHoldWritesPendingStatus(FunctionInfo& f)
{
	INM_ERROR errCode;
	errCode = E_SUCCESS ;
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
	return errCode ;
}
INM_ERROR TargetHandler::updateFlushAndHoldResumePendingStatus(FunctionInfo& f)
{
	INM_ERROR errCode;
	errCode = E_SUCCESS ;
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
	return errCode ;
}
INM_ERROR TargetHandler::getFlushAndHoldRequestSettings(FunctionInfo& f)
{
	INM_ERROR errCode;
	errCode = E_SUCCESS ;
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "0");
	return errCode ;
}
