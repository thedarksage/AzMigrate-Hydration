#include "discovery/discoveryfactory.h"
#include "oracle/Oraclediscoveryandvalidator.h"
#include "Array/arraydiscoveryandvalidator.h"
#include "db2/Db2discoveryandvalidator.h"

#include "config/appconfigurator.h"
#include "util/common/bulkprotectionandvalidator.h"

ApplicationPtr DiscoveryFactory::getApplication(InmProtectedAppType appType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    ApplicationPtr applicationPtr;

    switch(appType)
    {
        case INM_APP_ORACLE:
		applicationPtr.reset( new OracleApplication() );
		break;
        case INM_APP_DB2:
        applicationPtr.reset( new Db2Application() );
        break;

		
    }
    	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return applicationPtr ;
}

std::list<DiscoveryAndValidatorPtr> DiscoveryFactory::getDiscoveryAndValidator(SV_ULONG policyId, const std::set<std::string>& supportedApplicationNameSet)
{
    std::list<DiscoveryAndValidatorPtr> discoveryAndValidatorPtrList ;
    DiscoveryAndValidatorPtr discoveryValidatorPtr ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;

    std::set<std::string>::const_iterator supportedApplicationNameSetBegIter = supportedApplicationNameSet.begin();
    std::set<std::string>::const_iterator supportedApplicationNameSetEndIter = supportedApplicationNameSet.end();
    while(supportedApplicationNameSetBegIter != supportedApplicationNameSetEndIter)
    {
        if(strcmpi(supportedApplicationNameSetBegIter->c_str(), "BULK") == 0)
        {
            ApplicationPolicy bulkPolicy;
            if( configuratorPtr->getApplicationPolicies(policyId, "BULK", bulkPolicy) )
            {
                discoveryValidatorPtr.reset( new BulkProtectionAndValidator(bulkPolicy) ) ;
                discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;
            }
        }
        else if (strcmpi(supportedApplicationNameSetBegIter->c_str(), "SYSTEM") == 0 )
        {       
            ApplicationPolicy sysPolicy;
            if( configuratorPtr->getApplicationPolicies(policyId, "SYSTEM", sysPolicy) )
            {
                discoveryValidatorPtr.reset( new BulkProtectionAndValidator(sysPolicy) ) ;
                discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;
            }
        }
        else if (strcmpi(supportedApplicationNameSetBegIter->c_str(), "ARRAY") == 0)
        {
            ApplicationPolicy appPolicy;
            if(configuratorPtr->getApplicationPolicies(policyId, "ARRAY", appPolicy))
            {
                discoveryValidatorPtr.reset( new ArrayDiscoveryAndValidator(appPolicy) ) ;
                discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;
            }
        }
        else if(strcmpi(supportedApplicationNameSetBegIter->c_str(), "ORACLE") == 0)
        {
            ApplicationPolicy oraclePolicy;
            if(configuratorPtr->getApplicationPolicies(policyId, "ORACLE", oraclePolicy))
            {
                discoveryValidatorPtr.reset( new OracleDiscoveryAndValidator(oraclePolicy) ) ;
                discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;
            }
        }
        else if(strcmpi(supportedApplicationNameSetBegIter->c_str(), "DB2") == 0)
        {
            ApplicationPolicy db2Policy;
            if(configuratorPtr->getApplicationPolicies(policyId, "DB2", db2Policy))
            {
               discoveryValidatorPtr.reset( new Db2DiscoveryAndValidator(db2Policy) ) ;
               discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;
            }
        }
		else if(strcmpi(supportedApplicationNameSetBegIter->c_str(), "CLOUD") == 0)
		{
			//ApplicationPolicy cloudPolicy;
			//Currently there is no cloud validator.
		}
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Some un known application name is given in drscout.conf");
        }
        supportedApplicationNameSetBegIter++;
    }
    return discoveryAndValidatorPtrList ;
}

