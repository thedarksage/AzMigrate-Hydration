#include "appglobals.h"
#include <sstream>
#include <fstream>
#include <ace/File_Lock.h>

#include "appconfchangemediator.h"
#include "serializeapplicationsettings.h" 
#include "appconfigurator.h"
#include "controller.h"
#include "appexception.h"
#include "serializationtype.h"
#include "logger.h"

AppConfigChangeMediatorPtr AppConfigChangeMediator::m_instancePtr  ;

ConfigChangeMediator::ConfigChangeMediator(ACE_Thread_Manager* thrMgr)
:ACE_Task<ACE_MT_SYNCH>(thrMgr)
{
}
    
ConfigChangeMediatorPtr ConfigChangeMediator::CreateConfigChangeMediator()
{
    ConfigChangeMediatorPtr confChangeMediatorPtr ;
    confChangeMediatorPtr = AppConfigChangeMediator::GetInstance() ;
    return confChangeMediatorPtr ;
}

AppConfigChangeMediator::AppConfigChangeMediator(SV_UINT pollInterval, ACE_Thread_Manager* thrMgr)
:m_pollInterval(pollInterval),
ConfigChangeMediator(thrMgr)
{
}
AppConfigChangeMediatorPtr AppConfigChangeMediator::GetInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	if( m_instancePtr.get() == NULL )
    {
        throw "Initialize AppConfigChangeMediator object before calling getIntance" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_instancePtr ;
}

AppConfigChangeMediatorPtr AppConfigChangeMediator::CreateInstance(ACE_Thread_Manager* ptr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	if( m_instancePtr.get() == NULL )
    {
        AppLocalConfigurator localConfigurator ;
    	SV_UINT pollingInterval = localConfigurator.getAppSettingsPollingInterval() ;
        m_instancePtr.reset(new AppConfigChangeMediator(pollingInterval, ptr) ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_instancePtr ;
}

AppConfigChangeMediator::~AppConfigChangeMediator()
{
}

void AppConfigChangeMediator::LoadCachedSettings() 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    try
    {
        m_settings = AppAgentConfigurator::GetAppAgentConfigurator()->GetCachedApplicationSettings() ;
    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_DEBUG, "The cached settings are not invalid %s\n", ce.what()) ;
    }
    catch ( std::exception& e )
    {
        DebugPrintf(SV_LOG_DEBUG, "The cached settings are not invalid %s\n", e.what()) ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_DEBUG, "The cached settings are not valid\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

int AppConfigChangeMediator::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    LoadCachedSettings() ;
    PollConfigInfo() ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return( 0 );
}

void AppConfigChangeMediator::ProcessPendingUpdates()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    try
    {
           configuratorPtr->UpdatePendingAndFailedRequests();
    }
    catch(AppException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got exception %s\n", ex.to_string().c_str()) ;
    }
    catch(std::exception &ex1)
    {
        DebugPrintf(SV_LOG_ERROR, "Got exception %s\n", ex1.what()) ;
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void AppConfigChangeMediator::PollConfigInfo()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ControllerPtr controllerPtr = Controller::getInstance() ;
    do 
    {
        GetConfigInfoAndProcess();
		if( controllerPtr->QuitRequested(m_pollInterval) )
        {
            break ;
        }       

    }
    while( true ) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void AppConfigChangeMediator::GetConfigInfoAndProcess()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

        try
        {
	           GetApplicationSettings() ;
        AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
        configuratorPtr->getSerializationType();
        if (PHPSerialize == configuratorPtr->getSerializationType())
        {
	           ProcessPendingUpdates() ;
        }
    }
        catch(AppException &ex)
        {
            DebugPrintf(SV_LOG_ERROR, "Got exception %s\n", ex.to_string().c_str()) ;
        }
        catch(std::exception &ex1)
        {
            DebugPrintf(SV_LOG_ERROR, "Got exception %s\n", ex1.what()) ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Got exception\n") ;
        }
		
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}       



void AppConfigChangeMediator::GetApplicationSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    
    try
    {
		/* why this ?
        SetLogLevel( configurator.getLogLevel() );
        SetLogRemoteLogLevel( configurator.getRemoteLogLevel() );
		SetSerializeType( configurator.getSerializerType() ) ;
		*/ 

        ApplicationSettings currentSettings = configuratorPtr->GetApplicationSettings() ;
        DebugPrintf(SV_LOG_DEBUG, "Got current settings\n") ;

        m_settings = currentSettings ;
        m_applicationSettingsSignal(m_settings);
        configuratorPtr->CacheConsistencySettings(m_settings);
        configuratorPtr->SerializeApplicationSettings(m_settings) ;        
    }
    catch(AppException& exception)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Caught Exception %s\n", FUNCTION_NAME, exception.to_string().c_str());
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Caught exception: %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Caught unknown exception: %s\n", FUNCTION_NAME);
    }
    if((configuratorPtr->getConfigSource() == FETCH_SETTINGS_FROM_CX) 
            || (configuratorPtr->getConfigSource() == USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE))
    {
        ACE_Time_Value cur_time = ACE_OS::gettimeofday();
        if(((SV_ULONG)(cur_time.sec() - configuratorPtr->lastTimeSettingsGotFromCX().sec()) > m_settings.timeoutInSecs)
			|| (cur_time.sec() < configuratorPtr->lastTimeSettingsGotFromCX().sec()))	
        {
            if(remoteCxsReachable())
            {
                configuratorPtr->ChangeTransport(m_ReachableCx,PHPSerialize) ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}




sigslot::signal1<ApplicationSettings>& AppConfigChangeMediator::getApplicationSettingsSignal()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_applicationSettingsSignal;
}




int AppConfigChangeMediator::open(void *args)
{
    return this->activate(THR_BOUND);
}

int AppConfigChangeMediator::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}


bool AppConfigChangeMediator::getViableCxs(ViableCxs & viablecxs)
{
    REMOTECX_MAP::iterator iter = m_settings.remoteCxs.find("all");
    if(iter != m_settings.remoteCxs.end())
    {
        viablecxs.assign((iter ->second).begin(), (iter ->second).end());
        return true;
    }

    return false;
}

bool AppConfigChangeMediator::remoteCxsReachable()
{
    bool rv = false;

    REMOTECX_MAP::iterator iter = m_settings.remoteCxs.find("all");
    if(iter != m_settings.remoteCxs.end())
    {
        if(!(iter ->second).empty())
        {
            std::list<ViableCx>::iterator cxIter = iter->second.begin();
            for(; cxIter != iter->second.end(); ++cxIter)
            {
                if(AppAgentConfigurator::GetAppAgentConfigurator()->isCxReachable(cxIter->publicip, cxIter->port))
                {
                    m_ReachableCx.ip = cxIter->publicip ;
                    rv = true ;
                }
                else if(AppAgentConfigurator::GetAppAgentConfigurator()->isCxReachable(cxIter->privateip, cxIter->port))
                {
                    m_ReachableCx.ip = cxIter->privateip ;
                    rv = true ;
                }
                if( rv )
                {
                    m_ReachableCx.port = cxIter->port ;                    
                    break ;
                }
            }
        }
    }
    return rv;
}
