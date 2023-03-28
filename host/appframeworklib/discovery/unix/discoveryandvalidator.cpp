#include <boost/lexical_cast.hpp>

#include "discoveryandvalidator.h"
#include "applicationsettings.h"
#include "config/appconfigurator.h"
#include "appscheduler.h"
#include "controller.h"
#include "discovery/discoveryfactory.h"

DiscoveryAndValidator::DiscoveryAndValidator(const ApplicationPolicy& policy)
:m_policy(policy)
{
	m_scenarioType = SCENARIO_PROTECTION;
	m_context = CONTEXT_HADR;
    m_scheduleType = 0;
}

void DiscoveryAndValidator::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    const std::map<std::string, std::string>& policyProps = m_policy.m_policyProperties ;
    m_policyId = boost::lexical_cast<SV_ULONG>(policyProps.find("PolicyId")->second) ;
    m_policyType = policyProps.find( "PolicyType")->second ;
    if( policyProps.find( "InstanceName" ) !=  policyProps.end() )
    {
        m_appInstanceName = policyProps.find( "InstanceName" )->second ;
    }
    if( policyProps.find( "ApplicationType" ) !=  policyProps.end() )
    {
        m_applicationType = policyProps.find( "ApplicationType" )->second ;
    }
    if( policyProps.find( "SolutionType" ) !=  policyProps.end() )
    {
        m_solutionType = policyProps.find( "SolutionType" )->second ;
    }
    if( policyProps.find( "ScheduleType" ) !=  policyProps.end() )
    {
        m_scheduleType = boost::lexical_cast<SV_ULONG>(policyProps.find( "ScheduleType" )->second) ;
    }
	if( strcmpi(m_policyType.c_str(), "SourceReadinessCheck") == 0 || strcmpi(m_policyType.c_str(), "TargetReadinessCheck") == 0 )
	{
		if( policyProps.find( "ScenarioType" ) !=  policyProps.end() )
		{
			std::string scenarioType  = policyProps.find( "ScenarioType" )->second ;
			if( strcmpi(scenarioType.c_str(), "Protection") == 0 )
			{
				m_scenarioType = SCENARIO_PROTECTION;
			}
			else if( strcmpi(scenarioType.c_str(), "Failover") == 0 )
			{
				m_scenarioType = SCENARIO_FAILOVER;
			}
			else if( strcmpi(scenarioType.c_str(), "Failback") == 0 )
			{
				m_scenarioType = SCENARIO_FAILBACK;
			}
			else if( strcmpi(scenarioType.c_str(), "Fast-Failback") == 0 )
			{
				m_scenarioType = SCENARIO_FASTFAILBACK;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Invalid Scenario: %s \n", scenarioType.c_str());
			}
		}
		if( policyProps.find( "Context" ) !=  policyProps.end() )
		{
			std::string contextType = policyProps.find( "Context" )->second ;
			if( strcmpi(contextType.c_str(), "HA-DR") == 0 )
			{
				m_context = CONTEXT_HADR;
			}
			else if( strcmpi(contextType.c_str(), "Backup") == 0 )
			{
				m_context = CONTEXT_BACKUP;
			}
			else if( strcmpi(contextType.c_str(), "Bulk") == 0 || 
			         strcmpi(contextType.c_str(), "SYSTEM") == 0 )
			{
				m_context = CONTEXT_BULK;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Invalid Context: %s \n", contextType.c_str());
			}
		}
        if( policyProps.find( "TotalNumberOfPairs" ) !=  policyProps.end() )
        {
            m_totalNumberOfPairs = boost::lexical_cast<SV_ULONGLONG>(policyProps.find( "TotalNumberOfPairs" )->second);
            DebugPrintf(SV_LOG_DEBUG, "The Total Number of Pairs: %ld\n", m_totalNumberOfPairs) ;
        }
	}
	if(policyProps.find("ConsistencyCmd") != policyProps.end())
	{
		m_appVacpOptions = policyProps.find("ConsistencyCmd")->second;
	}
    if( m_appVacpOptions.compare("0") == 0 )
    {
        m_appVacpOptions = "" ;
    }
    
    if( m_applicationType.find("EXCHANGE") != std::string::npos && 
    	m_applicationType.compare("EXCHANGE2003") != 0 )
    {
        if( m_appVacpOptions.find("ExchangeIS") == std::string::npos )
        {
            m_appVacpOptions += " -w ExchangeIS" ;
        }
    }
    else  if(m_applicationType.find("SQL2005") == 0 )
    {
        if( m_appVacpOptions.find("-a SQL2005") == std::string::npos )
        {
            m_appVacpOptions += " -a SQL2005" ;
        }
    }
/*	else if( m_applicationType.find("FILESERVER") == 0 )
	{
        if( m_appVacpOptions.find("-a FS") == std::string::npos )
        {
            m_appVacpOptions += " -a FS" ;
        }	
	}*/

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR DiscoveryAndValidator::Process()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppSchedulerPtr appSchedPtr = AppScheduler::getInstance() ;

	if( !Controller::getInstance()->QuitRequested(1) )
	{
		appSchedPtr->UpdateTaskStatus(m_policyId,TASK_STATE_RUNNING);
		if( m_policyType.compare("Discovery") == 0)
		{
			Discover(true) ;
		}
		else if( m_policyType.compare("SourceReadinessCheck") == 0)
		{
			PerformReadinessCheckAtSource() ;
		}
		else if( m_policyType.compare("TargetReadinessCheck") == 0)
		{
			PerformReadinessCheckAtTarget() ;
		}
        else if( m_policyType.compare("UnregisterApplication") == 0)
        {
            UnregisterDB();
        }
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR DiscoveryAndValidator::fillPolicyInfoInfo(std::map<std::string, std::string>& policyInfo)
{
	SVERROR retStatus = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    policyInfo.insert(std::make_pair("policyId", boost::lexical_cast<std::string>(m_policyId))) ;
//    policyInfo.insert(std::make_pair("ApplicationInstanceId", m_appInstanceId)) ;
    policyInfo.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}
