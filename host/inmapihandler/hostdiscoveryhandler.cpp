/* TODO: remove unneeded headers */
#include <ace/Condition_Thread_Mutex.h>
#include <utility>
#include "hostdiscoveryhandler.h"
#include "apinames.h"
#include "inmstrcmp.h"
#include "portablehelpers.h"
#include "configurecxagent.h"
#include "util.h"
#include "host.h"
#include "ProtectionHandler.h"
#include "InmsdkGlobals.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/volumeconfig.h"
#include "confengine/protectionpairconfig.h"
#include "xmlizevolumegroupsettings.h"
#include "confengine/agentconfig.h"
#include "confengine/eventqueueconfig.h"

int HostDiscoveryHandler::count = 1;
HostDiscoveryHandler::HostDiscoveryHandler(void)
{
}

HostDiscoveryHandler::~HostDiscoveryHandler(void)
{
}

INM_ERROR HostDiscoveryHandler::process(FunctionInfo& f)
{
    Handler::process(f);
    INM_ERROR errCode = E_SUCCESS;
    if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, REGISTER_HOST_STATIC_INFO) == 0)
    {
        errCode = RegisterHost(f, StaticHostRegister);
    }
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, REGISTER_HOST_DYNAMIC_INFO) == 0)
    {
        errCode = RegisterHost(f, DynamicHostRegister);
    }
    else
    {
        //Through an exception that, the function is not supported.
    }
    return Handler::updateErrorCode(errCode);
}


INM_ERROR HostDiscoveryHandler::validate(FunctionInfo& f)
{
    INM_ERROR errCode = Handler::validate(f);
    if(E_SUCCESS != errCode)
        return errCode;

    //TODO: Write handler specific validation here

    return errCode;
}
void HostDiscoveryHandler::UpdateRepositoryDetails(std::string& hostname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if ( isHostNameChanged (hostname) || isRepoPathChanged() ) 
    {
        RemoveHostIDEntry();
        AddHostDetailsToRepositoryConf();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void HostDiscoveryHandler::UpdateAgentConfiguration(FunctionInfo &f)
{
    std::string hostId, driverVersion, hostname, ipaddress, osInfo, agentVersion ;
    std::string agentInstallPath, zone, sysVol, patchHistory, agentMode;
    std::string prod_version, processorarch ;
    ENDIANNESS e ;
    ParameterGroup ospg, cpuinfopg ;
    SV_ULONGLONG compatibilityNo ;
    OS_VAL osVal ;
    const char * const INDEX[] = {"1", "2", "3", "4", "5", "6", 
        "7", "8", "9", "10", "11", "12",
        "14", "15", "19", "20" };
    ParametersIter_t paramIter ;
    Parameters_t& params =  f.m_RequestPgs.m_Params ;
    bool proceed = true ;
    do
    {
        paramIter = params.find( INDEX[0] ) ;
        if( paramIter != params.end() )
        {
            hostId = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[1] ) ;
        if( paramIter != params.end() )
        {
            agentVersion = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[2] ) ;
        if( paramIter != params.end() )
        {
            driverVersion = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[3] ) ;
        if( paramIter != params.end() )
        {
            hostname = paramIter->second.value ;
            UpdateRepositoryDetails( hostname ) ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[4] ) ;
        if( paramIter != params.end() )
        {
            ipaddress = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }//osinfo
        ospg = f.m_RequestPgs.m_ParamGroups.find( INDEX[5] )->second ;

        paramIter = params.find( INDEX[6] ) ;
        if( paramIter != params.end() )
        {
            compatibilityNo = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[7] ) ;
        if( paramIter != params.end() )
        {
            agentInstallPath = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[8] ) ;
        if( paramIter != params.end() )
        {
            osVal = (OS_VAL)boost::lexical_cast<int>(paramIter->second.value) ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[9] ) ;
        if( paramIter != params.end() )
        {
            zone = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[10] ) ;
        if( paramIter != params.end() )
        {
            sysVol = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[11] ) ;
        if( paramIter != params.end() )
        {
            patchHistory = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[12] ) ;
        if( paramIter != params.end() )
        {
            agentMode = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[13] ) ;
        if( paramIter != params.end() )
        {
            prod_version = paramIter->second.value ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        paramIter = params.find( INDEX[14] ) ;

        if( paramIter != params.end() )
        {
            e = (ENDIANNESS)boost::lexical_cast<int>(paramIter->second.value) ;
        }
        else
        {
            proceed = false ;
            break ;
        }
        cpuinfopg = f.m_RequestPgs.m_ParamGroups.find( INDEX[15] )->second ;
    } while( 0 ) ;
    if( proceed )
    {
        AgentConfigPtr agentConfigPtr = AgentConfig::GetInstance() ;
        ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
        ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;

        agentConfigPtr->UpdateAgentConfiguration(hostId, agentVersion,driverVersion,
            hostname, ipaddress,
            ospg, compatibilityNo, 
            agentInstallPath, osVal,
            zone, sysVol,
            patchHistory, agentMode,
            prod_version, e, cpuinfopg ) ;
    }
}
bool HostDiscoveryHandler::registrationAllowed()
{
	bool bret = true ;
	ProtectionPairConfigPtr protectionpairConfigPtr = ProtectionPairConfig::GetInstance() ;
	if( protectionpairConfigPtr->MoveOperationInProgress() )
	{
		DebugPrintf(SV_LOG_ERROR, "The Move operation is in progress.. Host registration will be tried later\n") ;
		bret = false ;
	}
	return bret ;
}
INM_ERROR HostDiscoveryHandler::RegisterHost(FunctionInfo &f, const HostRegisterType_t regtype)
{
    /* TODO: update error code like exception coming 
    *       from extractfromreqresp and configurator
    *       store processing */
    INM_ERROR errCode = E_UNKNOWN;
    const char * const INDEX[] = {"13", "7"};
    /* since we are using same value as that of agent, 
    * this should not cause any issue; we should not be 
    * instantiating return structures. */
    ConfigureCxAgent::RegisterHostStatus_t st = ConfigureCxAgent::HostRegisterFailed;
    try
    {
        do
        {
            ParameterGroupsIter_t pgIter = f.m_RequestPgs.m_ParamGroups.find(INDEX[regtype]);
            if (f.m_RequestPgs.m_ParamGroups.end() == pgIter)
            {
                DebugPrintf(SV_LOG_ERROR, "could not find volume information in function info with name %s\n", INDEX[regtype]);
                errCode = E_INSUFFICIENT_PARAMS;
                break;
            }
            VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
			if( registrationAllowed() )
			{
				if( regtype == StaticHostRegister )
				{
					UpdateAgentConfiguration(f) ;
				}
				volumeConfigPtr->UpdateVolumes(pgIter->first, pgIter->second, regtype) ;
				ConfSchemaReaderWriterPtr confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
				EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
				eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

				confschemareaderwriterPtr->Sync() ;
				st = ConfigureCxAgent::HostRegistered;
			}
            errCode = E_SUCCESS;

        } while (0);
    }
    catch(const char* ex)
    {
        DebugPrintf(SV_LOG_ERROR, "RegisterHost failed with exception %s\n", ex) ;
    }
    catch(const std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "RegisterHost failed with exception %s\n", ex.what()) ;
    }
    catch(const std::string& st)
    {
        DebugPrintf(SV_LOG_ERROR, "RegisterHost failed with exception %s\n", st.c_str()) ;
    }
    /* TODO: instead of using C++ structure, put directly to response object */
    insertintoreqresp(f.m_ResponsePgs, cxArg ( st ), "1");

    return errCode;
}


