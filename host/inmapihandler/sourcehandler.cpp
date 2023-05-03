/* TODO: remove unneeded headers */
#include <utility>
#include "sourcehandler.h"
#include "apinames.h"
#include "inmstrcmp.h"
#include "portablehelpers.h"
#include "inmdefines.h"
#include <math.h>
#include "confengine/protectionpairconfig.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/alertconfig.h"
#include "confengine/eventqueueconfig.h"

SourceHandler::SourceHandler(void)
{
}

SourceHandler::~SourceHandler(void)
{
}

INM_ERROR SourceHandler::process(FunctionInfo& f)
{
	Handler::process(f);
	INM_ERROR errCode = E_SUCCESS;

    if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_VOLUMES_PENDING_CHANGES) == 0)
	{
		errCode = updateVolumesPendingChanges(f);
	} 
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SET_SOURCE_RESYNC_REQUIRED) == 0)
	{
		errCode = setSourceResyncRequired(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, BACKUPAGENT_PAUSE) == 0)
	{
		errCode = BackupAgentPause(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, BACKUPAGENT_PAUSE_TRACK) == 0)
	{
		errCode = BackupAgentPauseTrack(f);
	}
	else
	{
		//Through an exception that, the function is not supported.
	}
	return Handler::updateErrorCode(errCode);
}

INM_ERROR SourceHandler::BackupAgentPause(FunctionInfo& f)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS ;
	const char * const INDEX[] = {"2", "3" };
	CDP_DIRECTION direction ;
	std::string devicename ;
	ConstParametersIter_t paramIter ;
	paramIter = f.m_RequestPgs.m_Params.find( INDEX[0] ) ;
	direction = (CDP_DIRECTION)boost::lexical_cast<int>(paramIter->second.value); 
	paramIter = f.m_RequestPgs.m_Params.find( INDEX[1] ) ;
	devicename = paramIter->second.value ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	protectionPairConfigPtr->PauseProtection(direction, devicename) ;
	ConfSchemaReaderWriter::GetInstance()->Sync() ;
	insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return E_SUCCESS ;
}

INM_ERROR SourceHandler::BackupAgentPauseTrack(FunctionInfo& f)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS ;
	const char * const INDEX[] = {"2", "3" };
	CDP_DIRECTION direction ;
	std::string devicename ;
	ConstParametersIter_t paramIter ;
	paramIter = f.m_RequestPgs.m_Params.find( INDEX[0] ) ;
	direction = (CDP_DIRECTION)boost::lexical_cast<int>(paramIter->second.value); 
	paramIter = f.m_RequestPgs.m_Params.find( INDEX[1] ) ;
	devicename = paramIter->second.value ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	protectionPairConfigPtr->PauseTrack(direction, devicename) ;
	ConfSchemaReaderWriter::GetInstance()->Sync() ;
	insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return E_SUCCESS ;
}

INM_ERROR SourceHandler::validate(FunctionInfo& f)
{
	INM_ERROR errCode = Handler::validate(f);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write handler specific validation here

	return errCode;
}




INM_ERROR SourceHandler::updateVolumesPendingChanges(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    const char * const INDEX[] = {"2", "3" };
    INM_ERROR errCode = E_UNKNOWN;
    ConstParameterGroupsIter_t pendingChangesPGIter = f.m_RequestPgs.m_ParamGroups.find(INDEX[0]) ;
	ConstParameterGroupsIter_t UnavailableRpoVolPgIter = f.m_RequestPgs.m_ParamGroups.find(INDEX[1]) ;
    const ParameterGroup & volumePendingChangesPG = pendingChangesPGIter->second ;

    ConstParameterGroupsIter_t volumePendingChangesPGIter = volumePendingChangesPG.m_ParamGroups.begin();
    bool proceed = true ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
    ConstParametersIter_t pendingChangesParamIter ;
    while(volumePendingChangesPGIter != volumePendingChangesPG.m_ParamGroups.end())
    {
        const std::string & srcDevName = volumePendingChangesPGIter->first;
        const ParameterGroup &pendingChangesPG = volumePendingChangesPGIter->second;
        DebugPrintf(SV_LOG_DEBUG, "Updating Pending Changes for %s\n", srcDevName.c_str()) ;
        do
        {
            SV_ULONGLONG pendingChangesSize, currentTimeStamp, oldTimeStamp ;
            ValueType vt ;
            pendingChangesParamIter = pendingChangesPG.m_Params.find
                                    (NS_VolumeStats::totalChangesPending);
            if(pendingChangesParamIter != pendingChangesPG.m_Params.end())
            {
                vt = pendingChangesParamIter->second;
                pendingChangesSize = boost::lexical_cast<SV_ULONGLONG>(vt.value);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s param not found in pending changes pg\n", 
                                NS_VolumeStats::totalChangesPending) ;
                proceed = false ;
                break ;
            }
            pendingChangesParamIter = pendingChangesPG.m_Params.find
                                    (NS_VolumeStats::driverCurrentTimeStamp) ;
            if(pendingChangesParamIter != pendingChangesPG.m_Params.end())
            {
                vt = pendingChangesParamIter->second;
                currentTimeStamp = boost::lexical_cast<SV_ULONGLONG>(vt.value);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s param not found in pending changes pg\n", 
                                    NS_VolumeStats::driverCurrentTimeStamp) ;
                proceed = false ;
                break ;
            }
            pendingChangesParamIter = pendingChangesPG.m_Params.find
                                    (NS_VolumeStats::oldestChangeTimeStamp);
            if(pendingChangesParamIter != pendingChangesPG.m_Params.end())
            {
                vt = pendingChangesParamIter->second;
                oldTimeStamp = boost::lexical_cast<SV_ULONGLONG>(vt.value);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s param not found in pending changes pg\n", NS_VolumeStats::oldestChangeTimeStamp) ;
                proceed = false ;
                break ;
            }
            SV_ULONGLONG currentRPO = 0;
			if( oldTimeStamp != 0 || pendingChangesSize == 0 )
			{
				if(pendingChangesSize > 0)
				{
					currentRPO =  currentTimeStamp - oldTimeStamp;
				}
				SV_ULONGLONG rpoInterval = 10 * 10 * 10 * 10 * 10 * 10 * 10 ;
				double rpo = (1.0 * currentRPO)/ rpoInterval;
				rpo = floor(rpo+0.5);
				DebugPrintf(SV_LOG_DEBUG, "Pending Changes Size : %I64u, Current Time Stamp :%I64u Old Time Stamp :%I64u\n",
										pendingChangesSize, currentTimeStamp, oldTimeStamp) ;

				DebugPrintf(SV_LOG_DEBUG, "Current RPO : %I64u, RPO Interval %I64u, rpo After Transformation: %U64u\n",
										currentRPO, rpoInterval, rpo) ;
	            

				errCode = E_SUCCESS ;
				protectionPairConfigPtr->UpdatePendingChanges(srcDevName, 
					pendingChangesSize,
					rpo) ;
				double rpoInSec = 0 ; 
				if( protectionPairConfigPtr->GetRPOInSeconds(srcDevName, rpoInSec) )
				{
					if( rpoInSec > 30 * 60 )
					{
						std::string rpoInSecStr = protectionPairConfigPtr->ConvertRPOToHumanReadableformat (rpoInSec );
						std::string alertPrefix = "Current RPO time exceeds  SLA Threshold for " +  srcDevName + "." ;

						std::string alertMsg = alertPrefix +  " Current RPO is " + rpoInSecStr;
						alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg, alertPrefix) ;  // Error Level chaned to WARNING
					}
				}
			}
			else
			{
				protectionPairConfigPtr->NotValidRPO(srcDevName) ;
			}
        } while( 0 ) ;
        if( !proceed )
        {
            DebugPrintf(SV_LOG_ERROR, "Couldn't update the pending Changes for one/all of the volumes\n") ;
            break ;
        }
        volumePendingChangesPGIter++ ;
    }
	pendingChangesParamIter = UnavailableRpoVolPgIter->second.m_Params.begin() ; 
	while( pendingChangesParamIter != UnavailableRpoVolPgIter->second.m_Params.end() )
	{
		protectionPairConfigPtr->NotValidRPO(pendingChangesParamIter->second.value) ;
		pendingChangesParamIter++ ;
	}
    ConfSchemaReaderWriterPtr confschemareaderWriterPtr ;
    confschemareaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
    confschemareaderWriterPtr->Sync() ;
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return errCode  ;
}

INM_ERROR SourceHandler::setSourceResyncRequired(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    const char * const INDEX[] = {"2","3"};
    bool proceed = true ;
    INM_ERROR errCode = E_UNKNOWN;
    std::string srcDevName, errStr ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    do
    {
        ConstParametersIter_t paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 2 param in the requestPgs params list\n" ) ;
            proceed = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            errStr = paramIter->second.value ;
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
        protectionPairConfigPtr->SetResyncRequiredBySource( srcDevName, errStr ) ;
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

        ConfSchemaReaderWriterPtr confschemareaderWriterPtr ;
        confschemareaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
        confschemareaderWriterPtr->Sync() ;
        errCode = E_SUCCESS;
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( proceed ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return errCode ;
}