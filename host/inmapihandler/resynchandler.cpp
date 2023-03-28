/* TODO: remove unneeded headers */
#include "resynchandler.h"
#include "apinames.h"
#include "inmstrcmp.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/volumeconfig.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/eventqueueconfig.h"



ResyncHandler::ResyncHandler(void)
{
}

ResyncHandler::~ResyncHandler(void)
{
}

INM_ERROR ResyncHandler::process(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	Handler::process(f);
    INM_ERROR errCode = E_SUCCESS;

	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SET_RESYNC_TRANSITION) == 0)
	{
		errCode = setResyncTransitionStepOneToTwo(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_RESYNC_START_TIMESTAMP_FASTSYNC) == 0)
	{
		errCode = getResyncStartTimeStampFastsync(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SEND_RESYNC_START_TIMESTAMP_FASTSYNC) == 0)
	{
		errCode = sendResyncStartTimeStampFastsync(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_RESYNC_END_TIMESTAMP_FASTSYNC) == 0)
	{
		errCode = getResyncEndTimeStampFastsync(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SEND_RESYNC_END_TIMESTAMP_FASTSYNC) == 0)
	{
		errCode = sendResyncEndTimeStampFastsync(f);
	}
    else  if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SET_LAST_RESYNC_OFFSET_DIRECTSYNC) == 0)
	{
		errCode = setLastResyncOffsetForDirectSync(f);
	}
    else if( InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_CLEAR_DIFFS) == 0)
    {
        errCode = getClearDiffs(f);
    }
    else  if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, SET_RESYNC_PROGRESS_FASTSYNC) == 0)
	{
		errCode = setResyncProgressFastsync(f);
	}
	else
	{
		//Through an exception that, the function is not supported.
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return Handler::updateErrorCode(errCode);
}

INM_ERROR ResyncHandler::setResyncTransitionStepOneToTwo(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_INTERNAL;
    const char * const INDEX[] = {"3", "5", "6"};
    std::string srcDevName, tgtDevName, jobId ; 
    bool process = true ;
    ConstParametersIter_t paramIter ;
    do
    {
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 5 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            jobId = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 6 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    } while( 0 ) ;
    if( process )
    {
        VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
        SV_LONGLONG srcVolCapacity ;
        volumeConfigPtr->GetCapacity(srcDevName, srcVolCapacity) ;
        ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        if( protectionPairConfigPtr->MovePairFromStepOneToTwo(srcDevName, tgtDevName, srcVolCapacity, jobId) )
        {
			EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
			eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;
            ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
            confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
            confschemareaderwriterPtr->Sync() ;
            errCode = E_SUCCESS ;
        }
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}


INM_ERROR ResyncHandler::getResyncStartTimeStampFastsync(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_INTERNAL;
    bool process = true ;
    const char * const INDEX[] = {"3", "5","7","6"};
    std::string srcDevName, tgtDevName, operName, jobId ;
    ConstParametersIter_t paramIter ;
    do
    {
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 5 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            operName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 7 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[3]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            jobId = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 6 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    } while( 0 ) ;
    ParameterGroup responsePG ;
    if( process )
    {
        DebugPrintf(SV_LOG_DEBUG, "Operation Name %s\n", operName.c_str()) ;
        ProtectionPairConfigPtr protectioPairConfigPtr ;
        protectioPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        std::string TagTime, seqNo ;
        if( operName.compare("resyncStartTagtime") == 0 && 
            protectioPairConfigPtr->GetResyncStartTagTimeAndSeqNo(srcDevName, 
                                tgtDevName, jobId, TagTime, seqNo) )
        {
            errCode = E_SUCCESS ;
        }
        else if( protectioPairConfigPtr->GetResyncEndTagTimeAndSeqNo(srcDevName, tgtDevName, jobId,
                                                                TagTime, seqNo) )
        {
            errCode = E_SUCCESS ;
        }
        if( errCode == E_SUCCESS )
        {
            DebugPrintf(SV_LOG_DEBUG, "Tag Time %s. Seq No %s\n", TagTime.c_str(), seqNo.c_str()) ;
            insertintoreqresp(responsePG,cxArg(boost::lexical_cast<SV_ULONGLONG>(TagTime)),
                                                NS_ResyncTimeSettings::time);
            insertintoreqresp(responsePG,cxArg(boost::lexical_cast<SV_ULONGLONG>(seqNo)),
                                                NS_ResyncTimeSettings::seqNo);
        }
    }
    f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("1",responsePG));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}


INM_ERROR ResyncHandler::sendResyncStartTimeStampFastsync(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_INTERNAL;
    bool process = true ;
    const char * const INDEX[] = {"3", "5", "7","8","6"};
    std::string srcDevName, tgtDevName, timeStamp, seqNo, jobId ;

    do
    {
        ConstParametersIter_t paramIter ;
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 5 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            timeStamp = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 7 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[3]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            seqNo = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 8 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[4]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            jobId  = paramIter->second.value ;;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 6 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    } while( 0 ) ;
    DebugPrintf(SV_LOG_DEBUG, "srcDevName:%s, tgtDevName:%s, timeStamp:%s, seqNo:%s, jobId:%s\n", 
                srcDevName.c_str(), tgtDevName.c_str(), timeStamp.c_str(), seqNo.c_str(), jobId .c_str());
    if( process )
    {
        VolumeConfigPtr volumeConfigPtr ;
        volumeConfigPtr = VolumeConfig::GetInstance() ;
        ProtectionPairConfigPtr protectionPairConfigPtr ;
        protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        if( protectionPairConfigPtr->SetResyncStartTagTimeAndSeqNo(srcDevName, tgtDevName, 
                            timeStamp, seqNo, jobId) )
        {
            ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
            confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
            confschemareaderwriterPtr->Sync() ;
            errCode = E_SUCCESS ;
        }
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}


INM_ERROR ResyncHandler::getResyncEndTimeStampFastsync(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_INTERNAL;
    bool process = true ;
    const char * const INDEX[] = {"3", "5","6"};
    ParameterGroup responsePG ;
    std::string srcDevName, tgtDevName, jobId ;
    do
    {
        ConstParametersIter_t paramIter ;
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 5 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            jobId = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 6 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    } while( 0 ) ;
    if( process )
    {
        ProtectionPairConfigPtr protectionPairConfigPtr ;
        protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        std::string resyncEndTagTime, endSeqNo ;
        if( protectionPairConfigPtr->GetResyncEndTagTimeAndSeqNo(srcDevName, tgtDevName, jobId, 
                                        resyncEndTagTime, endSeqNo) )
        {
            insertintoreqresp(responsePG,cxArg(resyncEndTagTime),NS_ResyncTimeSettings::time);
            insertintoreqresp(responsePG,cxArg(endSeqNo),NS_ResyncTimeSettings::seqNo);
            errCode = E_SUCCESS ;
        }
    }
    f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("1",responsePG));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR ResyncHandler::sendResyncEndTimeStampFastsync(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_INTERNAL;
    bool process = true ;
    const char * const INDEX[] = {"3", "5", "7","8","6"};
    std::string srcDevName, tgtDevName, timeStamp, seqNo, jobId ;
    ConstParametersIter_t paramIter ;
    do
    {
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 5 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            timeStamp = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 7 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[3]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            seqNo = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 8 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[4]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            jobId = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 6 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    }while( 0 ) ;
    if( process )
    {
        ProtectionPairConfigPtr protectionPairConfigPtr ;
        protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        if( protectionPairConfigPtr->SetResyncEndTagTimeAndSeqNo(srcDevName, tgtDevName,
                                                             timeStamp, seqNo, jobId) )
        {
			EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
			eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;
            ConfSchemaReaderWriterPtr confschemareaderwriterptr;
            confschemareaderwriterptr = ConfSchemaReaderWriter::GetInstance() ;
            confschemareaderwriterptr->Sync() ;
            errCode = E_SUCCESS ;
        }
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR ResyncHandler::setLastResyncOffsetForDirectSync(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_INTERNAL;
    bool process = true ;
    const char * const INDEX[] = {"3", "5", "7","6", "8"};
    std::string srcDevName, tgtDevName, offset, jobId, fsunusedbytes ;
    do
    {
        ConstParametersIter_t paramIter ;
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 5 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            offset = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 7 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[3]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            jobId = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 6 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[4]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            fsunusedbytes = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 8 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    } while( 0 ) ;
    if( process )
    {
        ProtectionPairConfigPtr protectionPairConfigPtr ;
        protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        if( protectionPairConfigPtr->UpdateLastResyncOffset(srcDevName, tgtDevName, jobId, 
                                boost::lexical_cast<SV_LONGLONG>(offset),
								boost::lexical_cast<SV_LONGLONG>(fsunusedbytes)) )
        {
            ConfSchemaReaderWriterPtr confschemareaderwriterptr ;
            confschemareaderwriterptr = ConfSchemaReaderWriter::GetInstance() ;
            confschemareaderwriterptr->Sync() ;
            errCode = E_SUCCESS ;
        }
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}
INM_ERROR ResyncHandler::validate(FunctionInfo& f)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = Handler::validate(f);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write handler specific validation here
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return errCode;
}

INM_ERROR ResyncHandler::getClearDiffs(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = Handler::validate(f);
    errCode = E_SUCCESS ;
    insertintoreqresp(f.m_ResponsePgs, cxArg ( 1 ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode ;
}


INM_ERROR ResyncHandler::setResyncProgressFastsync(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_INTERNAL;
    bool process = true ;
    const char * const INDEX[] = {"3", "5", "7","8","9"};
    std::string srcDevName, tgtDevName, bytes, matched, jobId ;
    do
    {
        ConstParametersIter_t paramIter ;
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[0]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            srcDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 3 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[1]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            tgtDevName = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 5 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[2]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            bytes = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 7 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[3]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            matched = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 8 param in the params list\n" ) ;
            process = false ;
            break ;
        }
        paramIter = f.m_RequestPgs.m_Params.find(INDEX[4]) ;
        if( paramIter != f.m_RequestPgs.m_Params.end() )
        {
            jobId = paramIter->second.value ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "didnt find the 9 param in the params list\n" ) ;
            process = false ;
            break ;
        }
    } while( 0 ) ;
    if( process )
    {
        ProtectionPairConfigPtr protectionpairconfigptr ;
        protectionpairconfigptr = ProtectionPairConfig::GetInstance() ;
        if( protectionpairconfigptr->UpdateResyncProgress(srcDevName, tgtDevName, jobId, bytes, matched) )
        {
            ConfSchemaReaderWriterPtr confschemareaderwriterptr ;
            confschemareaderwriterptr = ConfSchemaReaderWriter::GetInstance() ;
            confschemareaderwriterptr->Sync() ;
            errCode = E_SUCCESS ;
        }
    }
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode ;
}