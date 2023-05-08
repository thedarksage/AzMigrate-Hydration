#include "vssutil.h"
#include "system_win.h"
#include <atlbase.h>
#include <Rpc.h>
#include<atlconv.h>
#include "boost/lexical_cast.hpp"
#include "util/common/util.h"
#include "controller.h"
#include "appcommand.h"

std::list<std::pair<std::string, std::string> > VssProviderProperties::getPropList() 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    USES_CONVERSION;
    std::list<std::pair<std::string, std::string> > providerPropList ;
	std::map<std::string, std::string>::iterator proverPropertiesTempIter;
	std::map<std::string, std::string>::iterator proverPropertiesEndIter;
	proverPropertiesEndIter = this->proverPropertiesMap.end();

	proverPropertiesTempIter = this->proverPropertiesMap.find("ProviderName");

	if( proverPropertiesTempIter != proverPropertiesEndIter )
	{
		providerPropList.push_back( std::make_pair( "Provider Name", proverPropertiesTempIter->second )) ;
	}
	
	proverPropertiesTempIter = this->proverPropertiesMap.find("ProviderType");

	if( proverPropertiesTempIter != proverPropertiesEndIter )
	{
		providerPropList.push_back(std::make_pair( "Provider Type", proverPropertiesTempIter->second )) ;
	}

	proverPropertiesTempIter = this->proverPropertiesMap.find("ProviderID");

	if( proverPropertiesTempIter != proverPropertiesEndIter )
	{
		providerPropList.push_back(std::make_pair( "Provider ID", proverPropertiesTempIter->second ));
	}

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return providerPropList ;
}

std::list<std::pair<std::string, std::string> > VSSWriterProperties::getPropList()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::pair<std::string, std::string> > properties ;
    properties.push_back(std::make_pair("Writer Name", m_writerName)) ;
    properties.push_back(std::make_pair("Writer Id", m_writerId)) ;
    properties.push_back(std::make_pair("Writer Instance Id", m_writerInstanceId)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return properties ;
}

SVERROR listVssProviders(std::list<VssProviderProperties>& providersList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	USES_CONVERSION;
	SVERROR bRet = SVS_FALSE ;
	SV_ULONG exitCode = 1;
	std::string outPutString;
    std::string strCommand ;
	bool bDisableRedirect = true;
	bool bDisableRedirectFlag = true;
    OSVERSIONINFOEX osVersionInfo ;
    getOSVersionInfo(osVersionInfo) ;
    if(osVersionInfo.dwMajorVersion >= 0x6)
    {
        strCommand = "C:\\Windows\\Sysnative\\vssadmin.exe list providers";
		bDisableRedirectFlag = false;
    }
    else
    {
        strCommand = "C:\\Windows\\system32\\vssadmin.exe list providers";
		bDisableRedirect = enableDiableFileSystemRedirection();
    }
	SV_INT timeOut = 600;
	std::string strOutputFile = generateLogFilePath(strCommand);
	AppCommand obj(strCommand,timeOut,strOutputFile);
	
	std::string providerNameString = "Provider name: ";
	std::string providerTypeString = "Provider type: ";
	std::string providerIdString = "Provider Id: ";
	std::string providerVersionString = "Version: ";

	std::string providerNameValue, providerTypeValue, providerIdValue, versionValue;
	std::string::size_type index1, index2, index3, index4;

	VssProviderProperties properties ;
    if(bDisableRedirect)
    {
		HANDLE h = NULL;
	    h = Controller::getInstance()->getDuplicateUserHandle();
	    if(obj.Run(exitCode,outPutString, Controller::getInstance()->m_bActive, "", h ) == SVS_OK)
	    {
            while(!outPutString.empty())
		    {
			    providerNameValue.clear();
			    providerTypeValue.clear();
			    providerIdValue.clear();
			    versionValue.clear();
			    index1 = index2 = index3 = std::string::npos;

			    index1 = outPutString.find(providerNameString);
			    index2 = outPutString.find(providerTypeString);
			    index3 = outPutString.find(providerIdString);
			    index4 = outPutString.find(providerVersionString);

			    if(index1 != std::string::npos && 
				    index2 != std::string::npos &&
				    index3 != std::string::npos &&
				    index4 != std::string::npos )
			    {
				    index1 += providerNameString.size();
				    if(index1 < index2)
				    {
					    providerNameValue = outPutString.substr(index1, (index2-index1) );
				    }
				    index2 += providerTypeString.size();
				    if(index2 < index3)
				    {
					    providerTypeValue = outPutString.substr(index2, (index3-index2) );
				    }
				    index3 += providerIdString.size();
				    if(index3 < index4)
				    {
					    providerIdValue = outPutString.substr(index3, (index4-index3) );
				    }
				    index4 += providerVersionString.size();
				    if(index4 < outPutString.size())
				    {
					    std::string::size_type nextIndex = outPutString.find(providerNameString, index4);
					    if(nextIndex != std::string::npos)
					    {
						    versionValue = outPutString.substr(index4, nextIndex-nextIndex);
					    }
					    else
					    {
						    versionValue = outPutString.substr(index4);
					    }
				    }
				    if((index4 + versionValue.size()) <= outPutString.size())
				    {
					    outPutString = outPutString.substr(index4 + versionValue.size() );
				    }

				    SIZE_T indexFirst,indexLast;

				    indexFirst = providerIdValue.find_first_of("{");
				    indexLast  = providerIdValue.find_first_of("}");

				    if((indexFirst != std::string::npos ) && (indexLast != std::string::npos))
				    {
					    providerIdValue = providerIdValue.substr(indexFirst+1,indexLast-1);
				    }
				    DebugPrintf(SV_LOG_INFO,"The provider Id value :%s \n", providerIdValue.c_str());
				    properties.proverPropertiesMap.insert(std::make_pair("ProviderID", providerIdValue));

				    //Removing leading and trailing whitespaces
				    indexFirst = providerTypeValue.find_first_not_of(" \t");
				    indexLast = providerTypeValue.find_last_not_of(" \t");
				    if( (indexFirst != std::string::npos) && (indexLast != std::string::npos))
				    {					
					    providerTypeValue = providerTypeValue.substr(indexFirst,indexLast-indexFirst+1);
				    }

				    //Removing trailing new line characters
				    indexFirst = providerTypeValue.find_first_of("\n");
				    if(indexFirst != std::string::npos)
				    {
					    providerTypeValue = providerTypeValue.substr(0,indexFirst-1);
				    }
				    DebugPrintf( SV_LOG_DEBUG, " The provider Type value :%s \n", providerTypeValue.c_str() );
				    properties.proverPropertiesMap.insert(std::make_pair("ProviderType", providerTypeValue));

				    indexFirst = providerNameValue.find_first_of("\n");
				    if(indexFirst != std::string::npos)
				    {
					    providerNameValue = providerNameValue.substr(0,indexFirst-1);
				    }

				    indexFirst = providerNameValue.find_first_of("'");
				    if(indexFirst != std::string::npos)
				    {
					    indexLast = providerNameValue.find_last_of("'");
					    if(indexLast != std::string::npos)
					    {
						    providerNameValue =  providerNameValue.substr(indexFirst+1,indexLast-1);
					    }
				    }
    				
				    DebugPrintf(SV_LOG_INFO,"providerNameValue = %s\n",providerNameValue.c_str());
				    properties.proverPropertiesMap.insert(std::make_pair("ProviderName", providerNameValue));
    				
				    providersList.push_back(properties);
			    }
			    else
			    {
				    outPutString.clear();
			    }
		    }
	    }
	    else
	    {
            DebugPrintf(SV_LOG_ERROR,"\n Failed to get the list of VSS providers.\n");
            DebugPrintf(SV_LOG_ERROR, "%s ",outPutString.c_str());
	    }
		if(h)
		{
			CloseHandle(h);
		}
		if(bDisableRedirectFlag)
		{
			bDisableRedirect = enableDiableFileSystemRedirection(false);
			if(bDisableRedirect)
			{
				DebugPrintf(SV_LOG_DEBUG,"Sucessfully reverted to original redirection\n");
			}
		}
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"\n Failed to redirect\n");
		
    }
	ACE_OS::unlink(strOutputFile.c_str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

HRESULT WaitAndCheckForAsyncOperation(IVssAsync *pva)
{
    HRESULT hInmResult = S_OK;
    HRESULT hrStatus = S_OK;
    do
    {
        hInmResult = pva->Wait() ;
        if( !FAILED(hInmResult) )
        {
            hInmResult = pva->QueryStatus(&hrStatus, NULL) ;
        }
    } while(false);
    return hInmResult;
}
