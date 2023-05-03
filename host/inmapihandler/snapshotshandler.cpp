#include "snapshotshandler.h"
#include <utility>
#include "volumesobject.h"
#include "configurecxagent.h"
#include "xmlmarshal.h"
#include "apinames.h"
#include "inmstrcmp.h"
#include "xmlizecdpsnapshotrequest.h"
#include "inmdefines.h"
#include "confschema.h"
#include "confschemamgr.h"
#include "inmsdkexception.h"
#include "util.h"
#include "InmsdkGlobals.h"
#include "inmageex.h"
#include "generalupdate.h"
#include "confengine/snapshotconfig.h"
#include "confengine/confschemareaderwriter.h"


INM_ERROR SnapShotHandler::getSrrJobs(FunctionInfo& f)
{
    INM_ERROR errCode ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    errCode = E_SUCCESS ;
    ParameterGroup pg ;
    f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("1", pg)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return errCode ;
}

INM_ERROR SnapShotHandler::process(FunctionInfo& f)
{
    Handler::process(f);
	INM_ERROR errCode = E_SUCCESS;

	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_SRR_JOBS) == 0)
	{
		errCode = getSrrJobs(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_SNAPSHOT_STATUS) == 0)
	{
		errCode = notifyCxOnSnapshotStatus(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_SNAPSHOT_PROGRESS) == 0)
	{
		errCode = notifyCxOnSnapshotProgress(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_SNAPSHOT_CREATION) == 0)
	{
		errCode = notifyCxOnSnapshotCreation(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, UPDATE_SNAPSHOT_DELETION) == 0)
	{
		errCode = notifyCxOnSnapshotDeletion(f);
	}
    
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, MAKE_ACTIVE_SNAPSHOT_INSTANCE) == 0)
	{
		errCode = makeSnapshotActive(f);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, DELETE_VIRTUAL_SNAPSHOT) == 0)
	{
		errCode = deleteVirtualSnapshot(f);
	}
    return errCode ;
}


INM_ERROR SnapShotHandler::notifyCxOnSnapshotStatus(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = (INM_ERROR)0 ;//Failure
    try
    {
      //  ValidateAndUpdateRequestPGToNotifyCxOnSnapshotStatus(f);
        errCode = (INM_ERROR)1; //Success
    }
    catch(GroupNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(InvalidArgumentException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaReadFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaUpdateFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
     catch(PairNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(AttributeNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(ParameterNotMatchedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(ContextualException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(const boost::bad_lexical_cast &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    //TODO: set proper error code in each exception. For now SUCCESS and FAILED
    insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR SnapShotHandler::notifyCxOnSnapshotProgress(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = (INM_ERROR)0 ;
    errCode = (INM_ERROR)1;
    insertintoreqresp(f.m_ResponsePgs, cxArg ( true ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR SnapShotHandler::makeSnapshotActive(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
     INM_ERROR errCode = (INM_ERROR)0 ;//Failure
    try
    {
       //ValidateAndUpdateRequestPGToMakeSnapshotActive(f);
        errCode = (INM_ERROR)1; //Success
    }
    catch(GroupNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(InvalidArgumentException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaReadFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaUpdateFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
     catch(PairNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(AttributeNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(ParameterNotMatchedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(ContextualException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(const boost::bad_lexical_cast &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    //TODO: set proper error code in each exception. For now SUCCESS and FAILED
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

INM_ERROR SnapShotHandler::notifyCxOnSnapshotCreation(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    ParameterGroup& vsnapPg = f.m_RequestPgs.m_ParamGroups.find( "2" )->second ;
	int noOfVsnapsInReq = vsnapPg.m_ParamGroups.size() ;
	ParameterGroup vsnapStatusPg ;
	for( int i = 0 ; i < noOfVsnapsInReq ; i++)
	{
		ValueType value ;
		value.value = "1" ;
		vsnapStatusPg.m_Params.insert( std::make_pair( "1[" + boost::lexical_cast<std::string>(i) + "]", value ) ) ;
	}
	f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( "1", vsnapStatusPg)) ; 
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return E_SUCCESS ;
}

INM_ERROR SnapShotHandler::notifyCxOnSnapshotDeletion(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = (INM_ERROR)0 ;//Failure
    
	ParameterGroup& vsnapPg = f.m_RequestPgs.m_ParamGroups.find( "2" )->second ;
	int noOfVsnapsInReq = vsnapPg.m_ParamGroups.size() ;
	ParameterGroup vsnapStatusPg ;
	for( int i = 0 ; i < noOfVsnapsInReq ; i++)
	{
		ValueType value ;
		value.value = "1" ;
		vsnapStatusPg.m_Params.insert( std::make_pair( "1[" + boost::lexical_cast<std::string>(i) + "]", value ) ) ;
	}
	f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( "1", vsnapStatusPg)) ;
    return errCode;
}

INM_ERROR SnapShotHandler::deleteVirtualSnapshot(FunctionInfo& f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = (INM_ERROR)0 ;//Failure
    try
    {
       // ValidateAndUpdateRequestPGToDeleteVirtualSnapshot(f);
        errCode = (INM_ERROR)1; //Success
    }
    catch(GroupNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(InvalidArgumentException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaReadFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaUpdateFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
     catch(PairNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(AttributeNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(ParameterNotMatchedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(ContextualException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(const boost::bad_lexical_cast &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    //TODO: set proper error code in each exception. For now SUCCESS and FAILED
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

