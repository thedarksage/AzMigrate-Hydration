#include "discovery/discoveryfactory.h"
#include "mssql/mssqldiscoveryandvalidator.h"
#include "exchange/exchangediscoveryandvalidator.h"
#include "fileserver/fileserverdiscoveryandvalidator.h"
#include "util/common/bulkprotectionandvalidator.h"
#include "config/appconfigurator.h"

ApplicationPtr DiscoveryFactory::getApplication(InmProtectedAppType appType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    ApplicationPtr applicationPtr;
    switch(appType)
    {
        case INM_APP_MSSQL :
            applicationPtr.reset( new MSSQLApplication() ) ;
            break ;
        case INM_APP_EXCHNAGE :
            applicationPtr.reset( new ExchangeApplication() ) ;
            break ;
		case INM_APP_FILESERVER :
			applicationPtr.reset( new FileServerAplication() );
			break;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return applicationPtr ;
}

std::list<DiscoveryAndValidatorPtr> DiscoveryFactory::getDiscoveryAndValidator(SV_ULONG policyId, const std::set<std::string>& selectedApplicationNameSet)
{
	std::list<DiscoveryAndValidatorPtr> discoveryAndValidatorPtrList ;
	DiscoveryAndValidatorPtr discoveryValidatorPtr ;
	bool bAddExchangeValidator = true, bAddSqlValidator = true, bAddBulkValidator = true;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    std::set<std::string>::const_iterator selectedApplicationNameSetBegIter = selectedApplicationNameSet.begin();
    std::set<std::string>::const_iterator selectedApplicationNameSetEndIter = selectedApplicationNameSet.end();
    while(selectedApplicationNameSetBegIter != selectedApplicationNameSetEndIter)
    {
        if( strcmpi(selectedApplicationNameSetBegIter->c_str(), "EXCHANGE") == 0 ||
			strcmpi(selectedApplicationNameSetBegIter->c_str(), "EXCHANGE2003") == 0 || 
			strcmpi(selectedApplicationNameSetBegIter->c_str(), "EXCHANGE2007") == 0 ||
			strcmpi(selectedApplicationNameSetBegIter->c_str(), "EXCHANGE2010") == 0)
        {
			ApplicationPolicy exchangePolicy;
			if(bAddExchangeValidator)
			{
				if( configuratorPtr->getApplicationPolicies(policyId, "EXCHANGE", exchangePolicy) )
				{
					DebugPrintf(SV_LOG_DEBUG, "Adding ExchangeDiscoveryAndValidator \n");
					discoveryValidatorPtr.reset( new ExchangeDiscoveryAndValidator(exchangePolicy) ) ;
					discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;	
					bAddExchangeValidator = false;
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "There are no exchange polices available for policy ID: %d \n", policyId);
				}
			}
        }
        else if(strcmpi(selectedApplicationNameSetBegIter->c_str(), "SQL") == 0 ||
				strcmpi(selectedApplicationNameSetBegIter->c_str(), "SQL2000") == 0 ||
				strcmpi(selectedApplicationNameSetBegIter->c_str(), "SQL2005") == 0 ||
				strcmpi(selectedApplicationNameSetBegIter->c_str(), "SQL2008") == 0 ||
				strcmpi(selectedApplicationNameSetBegIter->c_str(), "SQL2012") == 0 )
        {
			ApplicationPolicy mssqlPolicy;
			if(bAddSqlValidator)
			{
				if( configuratorPtr->getApplicationPolicies(policyId, "SQL", mssqlPolicy) )
				{
					DebugPrintf(SV_LOG_DEBUG, "Adding SQLDiscoveryAndValidator \n");
					discoveryValidatorPtr.reset( new SQLDiscoveryAndValidator(mssqlPolicy) ) ;
					discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;
					bAddSqlValidator = false;
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "There are no mssql polices available for policy ID: %d \n", policyId);
				}
			}
        }
        else if(strcmpi(selectedApplicationNameSetBegIter->c_str(), "FILESERVER") == 0)
        {
			ApplicationPolicy fileserverPolicy;
			if( configuratorPtr->getApplicationPolicies(policyId, "FILESERVER", fileserverPolicy) )
			{
				DebugPrintf(SV_LOG_DEBUG, "Adding FSDiscoveryAndValidator \n");
				discoveryValidatorPtr.reset( new FSDiscoveryAndValidator(fileserverPolicy) ) ;
				discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "There are no fileserver policy  available for policy ID: %d \n", policyId);
			}
        }
        else if(strcmpi(selectedApplicationNameSetBegIter->c_str(), "BULK") == 0 ||
                strcmpi(selectedApplicationNameSetBegIter->c_str(), "SYSTEM") == 0 )
        {
			ApplicationPolicy bulkPolicy;
			if(bAddBulkValidator)
			{
				if( configuratorPtr->getApplicationPolicies(policyId, "BULK", bulkPolicy) || 
					configuratorPtr->getApplicationPolicies(policyId, "SYSTEM", bulkPolicy) )
				{
					DebugPrintf(SV_LOG_DEBUG, "Adding BulkProtectionAndValidator \n");
					discoveryValidatorPtr.reset( new BulkProtectionAndValidator(bulkPolicy) ) ;
					discoveryAndValidatorPtrList.push_back(discoveryValidatorPtr) ;	
					bAddBulkValidator = false;
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "There are no BULK policy  available for policy ID: %d \n", policyId);
				}
			}
        }
		else if(strcmpi(selectedApplicationNameSetBegIter->c_str(), "CLOUD") == 0)
		{
			//ApplicationPolicy cloudPolicy;
			//Currently there is no cloud validator.
		}
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Unknow application name is given in drscout.conf.\n",selectedApplicationNameSetBegIter->c_str());
        }
        selectedApplicationNameSetBegIter++;
    }
    return discoveryAndValidatorPtrList ;
}
